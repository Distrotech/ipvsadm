/*
 *      ipvsadm - IP Virtual Server ADMinistration program
 *
 *      Version: $Id$
 *
 *      Authors: Wensong Zhang <wensong@iinchina.net>
 *               Peter Kese <peter.kese@ijs.si>
 *
 *      This program is based on ippfvsadm.
 *
 *      Changes:
 *        Wensong Zhang       :   added the editting service & destination support
 *        Wensong Zhang       :   added the feature to specify persistent port
 *        Jacob Rief          :   found the bug that masquerading dest of
 *                                different vport and dport cannot be deleted.
 *        Wensong Zhang       :   fixed it and changed some cosmetic things
 *        Wensong Zhang       :   added the timeout setting for persistent service
 *        Wensong Zhang       :   added specifying the dest weight zero
 *        Wensong Zhang       :   fixed the -E and -e options
 *        Wensong Zhang       :   added the long options
 *        Wensong Zhang       :   added the hostname and portname input
 *        Wensong Zhang       :   added the hostname and portname output
 *	  Lars Marowsky-Brée  :   added persistence granularity support
 *        Julian Anastasov    :   fixed the (null) print for unknown services
 *        Wensong Zhang       :   added the port_to_anyname function
 *        Horms               :   added option to read commands from stdin
 *        Horms               :   modified usage function so it prints to 
 *                            :   stdout if an exit value of 0 is used and 
 *                            :   stdout otherwise. Program is then terminated
 *                            :   with the supplied exit value.
 *        Horms               :   updated manpage and usage funtion so
 *                            :   the reflect the options available
 *        Wensong Zhang       :   added option to write rules to stdout
 *
 *      ippfvsadm - Port Fowarding & Virtual Server ADMinistration program
 *
 *      Copyright (c) 1998 Wensong Zhang
 *      All rights reserved.
 *
 *      Author: Wensong Zhang <wensong@iinchina.net>
 *
 *      This ippfvsadm is derived from Steven Clarke's ipportfw program.
 *
 *      portfw - Port Forwarding Table Editing v1.1
 *
 *      Copyright (c) 1997 Steven Clarke
 *      All rights reserved.
 *
 *      Author: Steven Clarke <steven@monmouth.demon.co.uk>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#undef __KERNEL__	/* Makefile lazyness ;) */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/param.h>
#include <arpa/inet.h>

#include <asm/types.h>          /* For __uXX types */
#include <net/if.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <linux/ip_fw.h>        /* For IP_FW_MASQ_CTL */
#include <linux/ip_masq.h>      /* For specific masq defs */
#include <net/ip_masq.h>
#include <net/ip_vs.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_POPT
#include "popt.h"
#include "config_stream.h"
#endif

/* default scheduler */
#define DEF_SCHED	"wlc"

/* printing format flags */
#define FMT_NONE	0x0000
#define FMT_NUMERIC	0x0001
#define FMT_RULE	0x0002

int string_to_number(const char *s, int min, int max);
int host_to_addr(const char *name, struct in_addr *addr);
char * addr_to_host(struct in_addr *addr);
char * addr_to_anyname(struct in_addr *addr);
int service_to_port(const char *name, unsigned short proto);
char * port_to_service(int port, unsigned short proto);
char * port_to_anyname(int port, unsigned short proto);
char * addrport_to_anyname(struct in_addr *addr, int port,
                           unsigned short proto, unsigned int format);

int parse_service(char *buf, u_int16_t proto, u_int32_t *addr, u_int16_t *port);
int parse_netmask(char *buf, u_int32_t *addr);
int parse_timeout(char *buf, unsigned *timeout);

void usage_exit(const char *program, const int exit_status);
void fail(int err, char *text);
void list_vs(unsigned int options);
void print_vsinfo(char *buf, unsigned int format);

int process_options(int argc, char **argv, int reading_stdin,
                    unsigned int options);


int main(int argc, char **argv)
{
        unsigned int options = FMT_NONE;

        /*
         *   If no other arguement, list /proc/net/ip_masq/vs
         */
        if (argc == 1){
                list_vs(options);
		exit(0);
	}
        
        /*
         *	Process command line arguments
         */
	return process_options(argc, argv, 0, options);
}


int process_options(int argc, char **argv, int reading_stdin, 
                    unsigned int options)
{
        struct ip_masq_ctl mc;
        int cmd, c;
        int parse;
        int result=0;
        int sockfd;
	int forward_set=0;
#ifdef HAVE_POPT
        int read_stdin=0;
        int write_stdout=0;
	dynamic_array_t *a;
	poptContext context;
	char *optarg=NULL;
	
        struct poptOption add_service_option =
                {"add-service", 'A', POPT_ARG_NONE, NULL, 'A'};
        struct poptOption edit_service_option =
                {"edit-service", 'E', POPT_ARG_NONE, NULL, 'E'};
        struct poptOption delete_service_option =
                {"delete-service", 'D', POPT_ARG_NONE, NULL, 'D'};
        struct poptOption clear_option =
                {"clear", 'C', POPT_ARG_NONE, NULL, 'C'};
        struct poptOption list_option =
                {"list", 'L', POPT_ARG_NONE, NULL, 'L'};
        struct poptOption list2_option =
                {"list", 'l', POPT_ARG_NONE, NULL, 'l'};
        struct poptOption add_server_option =
                {"add-server", 'a', POPT_ARG_NONE, NULL, 'a'};
        struct poptOption edit_server_option =
                {"edit-server", 'e', POPT_ARG_NONE, NULL, 'e'};
        struct poptOption delete_server_option =
                {"delete-server", 'd', POPT_ARG_NONE, NULL, 'd'};
        struct poptOption help_option =
                {"help", 'h', POPT_ARG_NONE, NULL, 'h'};
        struct poptOption read_stdin_option =
                {"restore", 'R', POPT_ARG_NONE, NULL, 'R'};
        struct poptOption write_stdout_option =
                {"save", 'S', POPT_ARG_NONE, NULL, 'S'};
        struct poptOption tcp_service_option =
                {"tcp-service", 't', POPT_ARG_STRING, &optarg, 't'};
        struct poptOption udp_service_option =
                {"udp-service", 'u', POPT_ARG_STRING, &optarg, 'u'};
        struct poptOption scheduler_option =
                {"scheduler", 's', POPT_ARG_STRING, &optarg, 's'};
        struct poptOption persistent_option =
                {"persistent", 'p', POPT_ARG_NONE, NULL, 'p'};
        struct poptOption netmask_option =
                {"netmask", 'M', POPT_ARG_STRING, &optarg, 'M'};
        struct poptOption real_server_option =
                {"real-server", 'r', POPT_ARG_STRING, &optarg, 'r'};
        struct poptOption real_server2_option =
                {"real-server", 'R', POPT_ARG_STRING, &optarg, 'R'};
        struct poptOption masquerading_option =
                {"masquerading", 'm', POPT_ARG_NONE, NULL, 'm'};
        struct poptOption ipip_option =
                {"ipip", 'i', POPT_ARG_NONE, NULL, 'i'};
        struct poptOption gatewaying_option =
                {"gatewaying",'g', POPT_ARG_NONE, NULL, 'g'};
        struct poptOption weight_option =
                {"weight", 'w', POPT_ARG_STRING, &optarg, 'w'};
        struct poptOption numeric_option =
                {"numeric", 'n', POPT_ARG_NONE, NULL, 'n'};
        struct poptOption NULL_option =
                {NULL, 0, 0, NULL, 0};

	struct poptOption *options_sub=NULL;

	struct poptOption options_main[] =
	{
		add_service_option,
		edit_service_option,
		delete_service_option,
		clear_option,
		list_option,
		list2_option,
		add_server_option,
		edit_server_option,
		delete_server_option,
		help_option,
		read_stdin_option,
		write_stdout_option,
		NULL_option
	};
		
	struct poptOption options_delete_service[] =
	{
		tcp_service_option,
		udp_service_option,
		NULL_option
	};

	struct poptOption options_service[] =
	{
		tcp_service_option,
		udp_service_option,
		scheduler_option,
		persistent_option,
		netmask_option,
		NULL_option
	};

	struct poptOption options_delete_server[] =
	{
		tcp_service_option,
		udp_service_option,
		weight_option,
		real_server_option,
		real_server2_option,
		NULL_option
	};

	struct poptOption options_server[] =
	{
		tcp_service_option,
		udp_service_option,
		weight_option,
		real_server_option,
		real_server2_option,
		gatewaying_option,
		masquerading_option,
		ipip_option,
		NULL_option
	};

	struct poptOption options_list[] =
	{
        	numeric_option,
		NULL_option
	};

	struct poptOption options_NULL[] =
	{
		NULL_option
	};

	context= poptGetContext("ipvsadm", argc, argv, options_main, 0);

        if ((cmd = poptGetNextOpt(context)) < 0)
                usage_exit(argv[0], -1);
#else
        char *optstr = "";

	struct option long_options[] =
	{
        	{"add-service", 0, 0, 'A'},
        	{"edit-service", 0, 0, 'E'},
        	{"delete-service", 0, 0, 'D'},
        	{"clear", 0, 0, 'C'},
        	{"list", 0, 0, 'L'},
        	{"add-server", 0, 0, 'a'},
        	{"edit-server", 0, 0, 'e'},
        	{"delete-server", 0, 0, 'd'},
        	{"help", 0, 0, 'h'},
        	{"tcp-service", 1, 0, 't'},
        	{"udp-service", 1, 0, 'u'},
        	{"scheduler", 1, 0, 's'},
        	{"persistent", 2, 0, 'p'},
        	{"real-server", 1, 0, 'r'},
        	{"masquerading", 0, 0, 'm'},
        	{"netmask", 1, 0, 'M'},
        	{"ipip", 0, 0, 'i'},
        	{"gatewaying", 0, 0, 'g'},
        	{"weight", 1, 0, 'w'},
        	{"numeric", 0, 0, 'n'},
        	{"help", 0, 0, 'h'},
        	{0, 0, 0, 0}
	};

	extern char *optarg;
	extern int optind;
	extern int opterr;
	extern int optopt;

	/* Re-process the arguments each time options is called*/
	optind = 1;

	if ((cmd = getopt_long(argc, argv, "AEDCaedlLh",
		long_options, NULL)) == EOF)
		usage_exit(argv[0], -1);
#endif

        memset(&mc, 0, sizeof(struct ip_masq_ctl));

        switch (cmd) {
        case 'A':	
                mc.m_cmd = IP_MASQ_CMD_ADD;
                mc.m_target = IP_MASQ_TARGET_VS;
#ifdef HAVE_POPT
		options_sub=options_service;
#else
                optstr = "t:u:s:M:p::";
#endif
                break;
        case 'E':	
                mc.m_cmd = IP_MASQ_CMD_SET;
                mc.m_target = IP_MASQ_TARGET_VS;
#ifdef HAVE_POPT
		options_sub=options_service;
#else
                optstr = "t:u:s:M:p::";
#endif
                break;
        case 'D':
                mc.m_cmd = IP_MASQ_CMD_DEL;
                mc.m_target = IP_MASQ_TARGET_VS;
#ifdef HAVE_POPT
		options_sub=options_delete_service;
#else
                optstr = "t:u:";
#endif
                break;
        case 'a':
                mc.m_cmd = IP_MASQ_CMD_ADD_DEST;
                mc.m_target = IP_MASQ_TARGET_VS;
#ifdef HAVE_POPT
		options_sub=options_server;
#else
                optstr = "t:u:w:r:R:gmi";
#endif
                break;
        case 'e':
                mc.m_cmd = IP_MASQ_CMD_SET_DEST;
                mc.m_target = IP_MASQ_TARGET_VS;
#ifdef HAVE_POPT
		options_sub=options_server;
#else
                optstr = "t:u:w:r:R:gmi";
#endif
                break;
        case 'd':
                mc.m_cmd = IP_MASQ_CMD_DEL_DEST;
                mc.m_target = IP_MASQ_TARGET_VS;
#ifdef HAVE_POPT
		options_sub=options_delete_server;
#else
                optstr = "t:u:w:r:R:";
#endif
                break;
        case 'C':
                mc.m_cmd = IP_MASQ_CMD_FLUSH;
                mc.m_target = IP_MASQ_TARGET_VS;
#ifdef HAVE_POPT
		options_sub=options_NULL;
#else
                optstr = "";
#endif
                break;
        case 'L':
        case 'l':
                mc.m_cmd = IP_MASQ_CMD_LIST;
                mc.m_target = IP_MASQ_TARGET_VS;
#ifdef HAVE_POPT
		options_sub=options_list;
#else
                optstr = "n";
#endif
                break;
#ifdef HAVE_POPT
	case 'R':
		read_stdin=1;
		options_sub=options_NULL;
		break;
	case 'S':
		write_stdout=1;
		options_sub=options_list;
		break;
#endif
	case 'h':
                usage_exit(argv[0], 0);
		break;
        default:
                usage_exit(argv[0], -1);
        }

        /*
         * weight=0 is allowed, which means that server is quiesced.
         */
        mc.u.vs_user.weight = -1;
        
        /*
         * Set direct routing as default forwarding method
         */
        mc.u.vs_user.masq_flags = IP_MASQ_F_VS_DROUTE;

        /*
         * Set the default persistent granularity to /32 masking
         */
        mc.u.vs_user.netmask	= ((unsigned long int) 0xffffffff);

#ifdef HAVE_POPT
	poptFreeContext(context);
	context = poptGetContext("ipvsadm", argc, argv, options_sub, 0);

	/* Discard the first argumet, which we have already paresed*/
        poptGetNextOpt(context);

        while ((c=poptGetNextOpt(context)) >= 0){
#else
	while ((c=getopt_long(argc, argv, optstr,
		long_options, NULL)) != EOF) {
#endif
                switch (c) {
                case 't':
                case 'u':
                        if (mc.u.vs_user.protocol != 0)
                                fail(2, "protocol already specified");
                        mc.u.vs_user.protocol =
                                (c=='t' ? IPPROTO_TCP : IPPROTO_UDP);
                        parse = parse_service(
						optarg,
						mc.u.vs_user.protocol,
						&mc.u.vs_user.vaddr, 
						&mc.u.vs_user.vport);
                        if (parse != 2) fail(2, "illegal virtual server "
                                             "address:port specified");
                        break;
                case 's':
                        if (strlen(mc.m_tname) != 0)
                                fail(2, "multiple scheduling modules specified");
                        strncpy(mc.m_tname, optarg, IP_MASQ_TNAME_MAX);
                        break;
                case 'p':
                        mc.u.vs_user.vs_flags = IP_VS_SVC_F_PERSISTENT;
#ifndef HAVE_POPT
                        if (!optarg && optind < argc &&
                            argv[optind][0] != '-' && argv[optind][0] != '!')
                                optarg = argv[optind++];
                        parse = parse_timeout(optarg,
                                              &mc.u.vs_user.timeout);
                        if (parse == 0) fail(2, "illegal timeout "
                                             "for persistent service");
#endif
                        break;
                case 'M':
                        parse = parse_netmask(optarg,
                                              &mc.u.vs_user.netmask);
                        if (parse != 1) fail(2, "illegal virtual server "
                                             "persistent mask specified");
                        break;
                case 'r':
                case 'R':
                        parse = parse_service(optarg,
                                              mc.u.vs_user.protocol,
                                              &mc.u.vs_user.daddr, 
                                              &mc.u.vs_user.dport);
                        if (parse == 0) fail(2, "illegal virtual server "
                                             "address:port specified");
                                /* copy vport to dport if none specified */
                        if (parse == 1)
                                mc.u.vs_user.dport = mc.u.vs_user.vport;
                        break;
                case 'i':
			if(forward_set)
                        	fail(2, "multiple forward mechanims set");
			forward_set=1;
                        mc.u.vs_user.masq_flags = IP_MASQ_F_VS_TUNNEL;
                        break;
                case 'g':
			if(forward_set)
                        	fail(2, "multiple forward mechanims set");
			forward_set=1;
                        mc.u.vs_user.masq_flags = IP_MASQ_F_VS_DROUTE;
                        break;
                case 'm':
			if(forward_set)
                        	fail(2, "multiple forward mechanims set");
			forward_set=1;
                        mc.u.vs_user.masq_flags = 0;
                        break;
                case 'w':
                        if (mc.u.vs_user.weight != -1)
                                fail(2, "multiple server weights specified");
                        if ((mc.u.vs_user.weight=
                             string_to_number(optarg,0,65535)) == -1)
                                fail(2, "illegal weight specified");
                        break;
                case 'n':
                        options |= FMT_NUMERIC;
                        break;
                default:
                        fail(2, "invalid option");
                }
        }

#ifdef HAVE_POPT
	if (c < -1) {
		/* an error occurred during option processing */
		fprintf(stderr, "%s: %s\n",
			poptBadOption(context, POPT_BADOPTION_NOALIAS),
			poptStrerror(c));
		poptFreeContext(context);
		return -1;
	}

	if (read_stdin) {
                /* avoid infinite loop */
		if (reading_stdin != 0)
                	usage_exit(argv[0], -1);

		while ((a = config_stream_read(stdin, argv[0])) != NULL) {
			int i;
			if ((i = (int)dynamic_array_get_count(a)) > 1)
				result = process_options(i, 
					(char **)dynamic_array_get_vector(a),
					1, options);
			dynamic_array_destroy(a, DESTROY_STR);
		}
		poptFreeContext(context);
		return(result);
	}

        if (write_stdout) {
                options |= FMT_RULE;
                list_vs(options);
		poptFreeContext(context);
                return 0;
        }
        
	/* If popt is used then optional arguments (persistent timeout)
	   has to be handled last. This has the interesting
	   side effect that the first non-option argument will
	   be used as the timeout, regardless of its position
	   in the argument list
	 */
	if (mc.u.vs_user.vs_flags == IP_VS_SVC_F_PERSISTENT){
		optarg=poptGetArg(context);
       		parse = parse_timeout(optarg, &mc.u.vs_user.timeout);
       		if (parse == 0) fail(2, "illegal timeout " 
			"for persistent service");
	}
#else
        if (optind < argc)
                fail(2, "unknown arguments found in command line");
#endif

        if (mc.m_target == IP_MASQ_TARGET_VS && mc.m_cmd == IP_MASQ_CMD_LIST) {
                list_vs(options);
#ifdef HAVE_POPT
		poptFreeContext(context);
#endif
		return(0);
	}

        if (mc.m_target == IP_MASQ_TARGET_VS &&
            (mc.m_cmd == IP_MASQ_CMD_ADD || mc.m_cmd == IP_MASQ_CMD_SET)) {
                /*
                 * Set the default scheduling algorithm if not specified
                 */
                if (strlen(mc.m_tname) == 0)
                        strcpy(mc.m_tname,DEF_SCHED);
        }

        if (mc.m_target == IP_MASQ_TARGET_VS
            && (mc.m_cmd == IP_MASQ_CMD_ADD_DEST
                || mc.m_cmd == IP_MASQ_CMD_SET_DEST)) {
                /*
                 * Set the default weight 1 if not specified
                 */
                if (mc.u.vs_user.weight == -1)
                        mc.u.vs_user.weight = 1;

                /*
                 * The destination port must be equal to the service port
                 * if the IP_MASQ_F_VS_TUNNEL or IP_MASQ_F_VS_DROUTE is set.
                 */
                if ((mc.u.vs_user.masq_flags == IP_MASQ_F_VS_TUNNEL)
                    || (mc.u.vs_user.masq_flags == IP_MASQ_F_VS_DROUTE))
                        mc.u.vs_user.dport = mc.u.vs_user.vport;
        }

        sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
        if (sockfd==-1) {
                perror("socket creation failed");
                exit(1);
        }

        result = setsockopt(sockfd, IPPROTO_IP, 
                            IP_FW_MASQ_CTL, (char *)&mc, sizeof(mc));
        if (result) {
                perror("setsockopt failed");

                /*
                 *    Print most common error messages
                 */
                switch (cmd) {
                case 'A':	
                        if (errno == EEXIST)
                                printf("Service already exists\n");
                        else if (errno == ENOENT)
                                printf("Scheduler not found: ip_vs_%s.o\n",
                                       mc.m_tname);
                        break;
                case 'E':
                        if (errno==ESRCH)
                                printf("No such service\n");
                        else if (errno == ENOENT)
                                printf("Scheduler not found: ip_vs_%s.o\n",
                                       mc.m_tname);
                        break;
                case 'D':
                        if (errno==ESRCH)
                                printf("No such service\n");
                        break;
                case 'a':
                        if (errno == ESRCH)
                                printf("Service not defined\n");
                        else if (errno == EEXIST)
                                printf("Destination already exists\n");
                        break;
                case 'e':
                        if (errno==ESRCH)
                                printf("Service not defined\n");
                        else if (errno == ENOENT)
                                printf("No such destination\n");
                        break;
                case 'd':
                        if (errno==ESRCH)
                                printf("Service not defined\n");
                        else if (errno == ENOENT)
                                printf("No such destination\n");
                        break;
                }
        }

        close(sockfd);

#ifdef HAVE_POPT
	poptFreeContext(context);
#endif
        return result;	
}


int string_to_number(const char *s, int min, int max)
{
	int number;
	char *end;

	number = (int)strtol(s, &end, 10);
	if (*end == '\0' && end != s) {
		/* we parsed a number, let's see if we want this */
		if (min <= number && number <= max)
			return number;
		else
			return -1;
	} else
		return -1;
}


/*
 * Get netmask.
 * Return 0 if failed,
 * 	  1 if addr read
 */
int parse_netmask(char *buf, u_int32_t *addr)
{
        struct in_addr inaddr;

	if(buf==NULL)
		return 0;
        
        if (inet_aton(buf, &inaddr) != 0)
                *addr = inaddr.s_addr;
        else if (host_to_addr(buf, &inaddr) != -1)
                *addr = inaddr.s_addr;
        else
                return 0;
        
        return 1;
}

/*
 * Get IP address and port from the argument. 
 * Return 0 if failed,
 * 	  1 if addr read
 *        2 if addr and port read
 */
int parse_service(char *buf, u_int16_t proto, u_int32_t *addr, u_int16_t *port)
{
        char *pp;
        long prt;
	struct in_addr inaddr;

	if(buf==NULL)
		return 0;
        
        pp = strchr(buf,':');
        if (pp) *pp = '\0';

        if (inet_aton(buf, &inaddr) != 0)
                *addr = inaddr.s_addr;
        else if (host_to_addr(buf, &inaddr) != -1)
                *addr = inaddr.s_addr;
        else                
		return 0;
        
        if (pp == NULL)
                return 1;
        
        if ((prt=string_to_number(pp+1, 0, 65535)) != -1)
                *port = htons(prt);
        else if ((prt=service_to_port(pp+1, proto)) != -1)
                *port = htons(prt);
        else
                return 0;

        return 2;
}


/*
 * Get the timeout of persistent service.
 * Return 0 if failed(the timeout value is less or equal than zero).
 * 	  1 if succeed.
 */
int parse_timeout(char *buf, unsigned *timeout)
{
        int i;

        if (buf == NULL) {
                *timeout = IP_VS_TEMPLATE_TIMEOUT;
                return 1;
        }
        
        if ((i=string_to_number(buf, 1, 86400*31)) == -1)
                return 0;

        *timeout = i * HZ;
        return 1;
}


void usage_exit(const char *program, const int exit_status) {
        FILE *stream;

	if (exit_status != 0)
		stream = stderr;
	else
		stream = stdout;

        fprintf(stream,
                "ipvsadm  v1.8 2000/03/13"
#ifdef HAVE_POPT
                " (popt)\n"
#else
                " (getopt_long)\n"
#endif
                "Usage: %s -[A|E] -[t|u] service-address [-s scheduler] [-p [timeout]] [-M netmask]\n"
                "       %s -D -[t|u] service-address\n"
                "       %s -C\n"
#ifdef HAVE_POPT
                "       %s -R\n"
                "       %s -S [-n]\n"
#endif
                "       %s -[a|e] -[t|u] service-address -[r|R] server-address [-g|-i|-m] [-w weight]\n"
                "       %s -d -[t|u] service-address -[r|R] server-address\n"
                "       %s -[L|l] [-n]\n"
                "       %s -h\n\n",
#ifdef HAVE_POPT
                program, program,
#endif
                program, program, program, program, program, program, program);
        
        printf("Commands:\n"
               "Either long or short options are allowed.\n"
               "  --add-service     -A        add virtual service with options\n"
               "  --edit-service    -E        edit virtual service with options\n"
               "  --delete-service  -D	      delete virtual service\n"
               "  --clear           -C        clear the whole table\n"
#ifdef HAVE_POPT
               "  --restore         -R	      restore rules from stdin\n"
               "  --save            -S	      save rules to stdout\n"
#endif
               "  --add-server      -a        add real server with options\n"
               "  --edit-server     -e        edit real server with options\n"
               "  --delete-server   -d        delete real server\n"
               "  --list            -L|-l     list the table\n"
               "  --help            -h	      display this help message\n\n"
               );
        
        fprintf(stream, 
		"Options:\n"
                "  --tcp-service  -t service-address   service-address is host and port\n"
                "  --udp-service  -u service-address   service-address is host and port\n"
                "  --scheduler    -s <scheduler>       It can be rr|wrr|lc|wlc,\n"
                "                                      the default scheduler is %s.\n"
                "  --persistent   -p [timeout]         persistent service\n"
                "  --netmask      -M [netmask]         persistent granularity mask\n"
                "  --real-server  -r|-R server-address server-address is host (and port)\n"
                "  --gatewaying   -g                   gatewaying (direct routing) (default)\n"
                "  --ipip         -i                   ipip encapsulation (tunneling)\n"
                "  --masquerading -m                   masquerading (NAT)\n"
                "  --weight:      -w <weight>          capacity of real server\n"
                "  --numeric      -n                   numeric output of addresses and ports\n",
                DEF_SCHED);
        
        exit(exit_status);
}


void fail(int err, char *text) {
        printf("%s\n",text);
        exit(err);
}


void list_vs(unsigned int format)
{
        static char buffer[1024];
        FILE *handle;
        int i;

        handle = fopen("/proc/net/ip_masq/vs", "r");
        if (!handle) {
                printf("Could not open /proc/net/ip_masq/vs\n");
                printf("Are you sure that Virtual Server is supported by the kernel?\n");
                exit(1);
        }

        /*
         * Read and print the first three head lines
         */
        for (i=0; i<2 && !feof(handle); i++) {
                if (fgets(buffer, sizeof(buffer), handle)
                    && !(format & FMT_RULE))
                        printf("%s", buffer);
        }
        if (fgets(buffer, sizeof(buffer), handle) && !(format & FMT_RULE))
                printf("  -> RemoteAddress:Port          "
                       "Forward Weight ActiveConn InActConn\n");
        
        /*
         * Print the VS information according to the format
         */
        while (!feof(handle)) {
                if (fgets(buffer, sizeof(buffer), handle))
                        print_vsinfo(buffer, format);
        }

        fclose(handle);
}


char *get_fwd_switch(char *fwd) 
{
        char *swt;
        
        switch (fwd[0]) {
        case 'M':
                swt = "-m"; break;
        case 'T':
                swt = "-i"; break;
        default:
                swt = "-g"; break;
        }
        return swt;
}
        
void print_vsinfo(char *buf, unsigned int format)
{
        /* virtual service variables */
        char protocol[10];
        static unsigned short  proto = 0;
        static struct in_addr  vaddr;
        static unsigned short  vport;
        char scheduler[10];
        char flags[40];
        unsigned int timeout;
        struct in_addr  vmask;

        /* destination variables */
        static char arrow[10];
        struct in_addr	daddr;
        unsigned short	dport;
        static char fwd[10];
        int weight;
        int activeconns;
        int inactconns;
        
        int n;
        unsigned long temp;
	unsigned long temp2;
        char *vname;
        char *dname;
	
        if (buf[0] == ' ') {
                /* destination entry */
                if ((n = sscanf(buf, " %s %lX:%hX %s %d %d %d",
                                arrow, &temp, &dport, fwd, &weight,
                                &activeconns, &inactconns)) == -1)
                        exit(1);
                if (n != 7)
                        fail(2, "unexpected input data");
                
                daddr.s_addr = (__u32) htonl(temp);

                if (!(dname=addrport_to_anyname(&daddr,dport,proto,format)))
                        exit(1);
                
                if (format & FMT_RULE) {
                        if (!(vname=addrport_to_anyname
                              (&vaddr,vport,proto,format)))
                                exit(1);
                                
                        printf("-a %s %s -r %s %s -w %d\n",
                               proto==IPPROTO_TCP?"-t":"-u",
                               vname, dname, get_fwd_switch(fwd), weight);
                        free(vname);
                } else
                        printf("  -> %-27s %-7s %-6d %-10d %-10d\n",
                               addrport_to_anyname(&daddr,dport,proto,format),
                               fwd, weight, activeconns, inactconns);
                free(dname);
        } else {
                /* virtual service entry */
                if ((n = sscanf(buf, "%s %lX:%hX %s %s %d %lX",
                                protocol, &temp, &vport, scheduler,
                                flags, &timeout, &temp2)) == -1)
                        exit(1);
                if (n!=7 && n!=4)
                        fail(2, "unexpected input data");

                vaddr.s_addr = (__u32) htonl(temp);
                vmask.s_addr = (__u32) htonl(temp2);
                if (strcmp(protocol, "TCP") == 0) proto = IPPROTO_TCP;
                else if (strcmp(protocol, "UDP") == 0) proto = IPPROTO_UDP;
                else proto = 0;

                if (!(vname=addrport_to_anyname(&vaddr,vport,proto,format)))
                        exit(1);

                if (format & FMT_RULE)
                        printf("-A %s %s -s %s", proto==IPPROTO_TCP?"-t":"-u",
                               vname, scheduler);
                else
                        printf("%s  %s %s", protocol, vname, scheduler);
                
                if (n == 4)
                        printf("\n");
                else {
                        printf(" %s %d",
                               format&FMT_RULE?"-p":flags, timeout/HZ);

                        if (vmask.s_addr == (unsigned long int) 0xffffffff)
                                printf("\n");
                        else
                                printf(" %s %s\n",
                                       format&FMT_RULE?"-M":"mask",
                                       inet_ntoa(vmask));
                }
                free(vname);
        }
}


int host_to_addr(const char *name, struct in_addr *addr)
{
        struct hostent *host;

        if ((host = gethostbyname(name)) != NULL) {
                if (host->h_addrtype != AF_INET ||
                    host->h_length != sizeof(struct in_addr))
                        return -1;
                /* warning: we just handle h_addr_list[0] here */
                memcpy(addr, host->h_addr_list[0], sizeof(struct in_addr));
                return 0;
        }
        return -1;
}


char * addr_to_host(struct in_addr *addr)
{
        struct hostent *host;

        if ((host = gethostbyaddr((char *) addr,
                                  sizeof(struct in_addr), AF_INET)) != NULL)
                return (char *) host->h_name;
        else
                return (char *) NULL;
}


char * addr_to_anyname(struct in_addr *addr)
{
        char *name;

        if ((name = addr_to_host(addr)) != NULL)
                return name;
        else
                return inet_ntoa(*addr);
}


int service_to_port(const char *name, unsigned short proto)
{
        struct servent *service;

        if (proto == IPPROTO_TCP
            && (service = getservbyname(name, "tcp")) != NULL)
                return ntohs((unsigned short) service->s_port);
        else if (proto == IPPROTO_UDP
                 && (service = getservbyname(name, "udp")) != NULL)
                return ntohs((unsigned short) service->s_port);
        else
                return -1;
}


char * port_to_service(int port, unsigned short proto)
{
        struct servent *service;

        if (proto == IPPROTO_TCP &&
            (service = getservbyport(htons(port), "tcp")) != NULL)
                return service->s_name;
        else if (proto == IPPROTO_UDP &&
                 (service = getservbyport(htons(port), "udp")) != NULL)
                return service->s_name;
        else
                return (char *) NULL;
}


char * port_to_anyname(int port, unsigned short proto)
{
        char *name;
        static char buf[10];

        if ((name = port_to_service(port, proto)) != NULL)
                return name;
        else {
                snprintf(buf, 10, "%d", port);
                return buf;
        }
}


char * addrport_to_anyname(struct in_addr *addr, int port, unsigned short proto, unsigned int format)
{
        char *buf;

        if (!(buf=malloc(60)))
                return NULL;
        
        if (format & FMT_NUMERIC) {
                snprintf(buf, 60, "%s:%u",
                         inet_ntoa(*addr), port);
        } else {
                snprintf(buf, 60, "%s:%s", addr_to_anyname(addr),
                         port_to_anyname(port, proto));
        }

        return buf;
}



