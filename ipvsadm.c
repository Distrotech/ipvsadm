/*
 *      ipvsadm - IP Virtual Server ADMinistration program
 *
 *      Version: $Id$
 *
 *      Authors: Peter Kese <peter.kese@ijs.si>
 *               Wensong Zhang <wensong@iinchina.net>
 *
 *      This program is based on ippfvsadm.
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
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
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

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>

#define DEF_SCHED	"wlc"

int parse_addr(char *buf, u_int32_t *addr, u_int16_t *port);
void usage_exit(char **argv);
void fail(int err, char *text);
void list_forwarding_exit(void);


int main(int argc, char **argv)
{
	struct ip_masq_ctl mc;
	int cmd, c;
	int pares;
	char *optstr;
	int result;
	int sockfd;

        /*
         *   If no other arguement, list /proc/net/ip_masq/vs
         */
        if (argc == 1)
                list_forwarding_exit();
        
	memset (&mc, 0, sizeof(struct ip_masq_ctl));
        
        /*
         * weight=0 is allowed, which means server not available
         * will be implemented in the future.
         */
        /*  mc.u.vs_user.weight = -1; */
        
	/*
	 *	Want user virtual server control
	 */
	if ((cmd = getopt (argc, argv, "AEDCaedlLh")) == EOF) 
		usage_exit(argv);

	switch (cmd) {
                case 'A':	
                        mc.m_cmd = IP_MASQ_CMD_ADD;
                        mc.m_target = IP_MASQ_TARGET_VS;
                        optstr = "t:u:s:p";
                        break;
                case 'E':	
                        mc.m_cmd = IP_MASQ_CMD_SET;
                        mc.m_target = IP_MASQ_TARGET_VS;
                        optstr = "t:u:s:";
                        break;
                case 'D':
                        mc.m_cmd = IP_MASQ_CMD_DEL;
                        mc.m_target = IP_MASQ_TARGET_VS;
                        optstr = "t:u:";
                        break;
                case 'a':
                        mc.m_cmd = IP_MASQ_CMD_ADD_DEST;
                        mc.m_target = IP_MASQ_TARGET_VS;
                        optstr = "t:u:w:r:R:gmi";
                        break;
                case 'e':
                        mc.m_cmd = IP_MASQ_CMD_SET_DEST;
                        mc.m_target = IP_MASQ_TARGET_VS;
                        optstr = "t:u:w:r:R:gmi";
                        break;
                case 'd':
                        mc.m_cmd = IP_MASQ_CMD_DEL_DEST;
                        mc.m_target = IP_MASQ_TARGET_VS;
                        optstr = "t:u:w:r:R:";
                        break;
                case 'C':
                        mc.m_cmd = IP_MASQ_CMD_FLUSH;
                        mc.m_target = IP_MASQ_TARGET_VS;
                        optstr = "";
                        break;
                case 'L':
                case 'l':
                        list_forwarding_exit();
                default:
                        usage_exit(argv);
	}

	/* use direct routing as default forwarding method */
	mc.u.vs_user.masq_flags = IP_MASQ_F_VS_DROUTE;
	
	while ((c = getopt (argc, argv, optstr)) != EOF) {
	        switch (c) {
                        case 't':
                        case 'u':
                                if (mc.u.vs_user.protocol != 0)
                                        fail(2, "protocol already specified");
                                mc.u.vs_user.protocol=(c=='t' ? IPPROTO_TCP : IPPROTO_UDP);
                                pares = parse_addr(optarg, &mc.u.vs_user.vaddr, 
                                                   &mc.u.vs_user.vport);
                                if (pares != 2) fail(2, "illegal virtual server "
                                                     "address:port specified");
                                break;
                        case 'p':
                                mc.u.vs_user.vs_flags = IP_VS_F_PERSISTENT;
                                break;
                        case 'i':
                                mc.u.vs_user.masq_flags = IP_MASQ_F_VS_TUNNEL;
                                break;
                        case 'g':
                                mc.u.vs_user.masq_flags = IP_MASQ_F_VS_DROUTE;
                                break;
                        case 'm':
                                mc.u.vs_user.masq_flags = 0;
                                break;
                        case 'r':
                        case 'R':
                                pares = parse_addr(optarg, &mc.u.vs_user.daddr, 
                                                   &mc.u.vs_user.dport);
                                if (pares == 0) fail(2, "illegal virtual server "
                                                     "address:port specified");
                                /* copy vport to dport if none specified */
                                if (pares == 1) mc.u.vs_user.dport = mc.u.vs_user.vport;
                                break;
                        case 'w':
                                if (mc.u.vs_user.weight != 0)
                                        fail(2, "multiple server weights specified");
                                if (sscanf(optarg, "%d", &mc.u.vs_user.weight) != 1)
                                        fail(2, "illegal weight specified");
                                break;
                        case 's':
                                if (strlen(mc.m_tname) != 0)
                                        fail(2, "multiple scheduling modules specified");
                                strncpy(mc.m_tname, optarg, IP_MASQ_TNAME_MAX);
                                break;
                        default:
                                fail(2, "invalid option");
		}
	}

	if (optind < argc)
                fail(2, "unknown arguments found in command line");

	/* set the default scheduling algorithm if none specified */
	if (mc.m_target == IP_MASQ_TARGET_VS && mc.m_cmd == IP_MASQ_CMD_ADD
	    && strlen(mc.m_tname) == 0) {
	        strcpy(mc.m_tname,DEF_SCHED);
	}

        /*
         * Set the default weight 1
         */
        if (mc.u.vs_user.weight == 0)
                mc.u.vs_user.weight = 1;

        /*
         * The destination port must be equal to the service port if the
         * IP_MASQ_F_VS_TUNNEL or IP_MASQ_F_VS_DROUTE is set.
         */
        if ((mc.u.vs_user.masq_flags == IP_MASQ_F_VS_TUNNEL)
            || (mc.u.vs_user.masq_flags == IP_MASQ_F_VS_DROUTE))
                mc.u.vs_user.dport = mc.u.vs_user.vport;
	
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
	return result;	
}


/* get ip address from the argument. 
 * Return 0 if failed,
 * 	  1 if addr read
 *        2 if addr and port read
 */
int parse_addr(char *buf, u_int32_t *addr, u_int16_t *port)
{
	char *pp;
	long prt;
	
	pp = strchr(buf,':');
	if (pp) *pp = '\0';

	*addr = inet_addr(buf);
	if (*addr == (u_int32_t)-1)
                return 0;
	if (pp == NULL)
                return 1;
	prt = atoi(++pp);
	if ((prt < 0) || (prt > 65535))
                return 0;
	*port = htons(prt);
	return 2;
}


void usage_exit(char **argv) {
	char *p = argv[0];
	printf("ipvsadm  v1.2 1999/9/1\n"
               "Usage:\n"
	       "\t%s -A -[t|u] v.v.v.v:vport [-s scheduler] [-p]\n"
	       "\t%s -D -[t|u] v.v.v.v:vport\n"
	       "\t%s -C\n"
	       "\t%s -a -[t|u] v.v.v.v:vport -r d.d.d.d[:dport] [-g|-m|-i]"
	       "[-w wght]\n"
	       "\t%s -d -[t|u] v.v.v.v:vport -r d.d.d.d[:dport]\n"
	       "\t%s [-L|-l]\n",p,p,p,p,p,p);
	printf("\nCommands:\n"
	       "\t -A	add virtual service and link a scheduler to it\n"
	       "\t -D	delete virtual service\n"
	       "\t -C	clear the whole table\n"
	       "\t -a	add real server and select forwarding method\n"
	       "\t -e	edit real server\n"
	       "\t -d	delete real server\n"
	       "\t -L	list the table\n");
	printf("\nOptions:\n"
	       "\t protocol:	t :	tcp\n"
	       "\t		u :	udp\n"
	       "\t port:	p :	persistent port\n"
	       "\t forwarding:	g :	gatewaying (routing) (default)\n"
	       "\t		m :	masquerading\n"
	       "\t		i :	ipip encapsulation (tunneling)\n"
	       "\t scheduler:	scheduling module 'ip_vs_<scheduler>.o'\n"
	       "\t 		default scheduler is '%s'.\n"
	       "\t weight:	capacity of the real server\n"
	       "\t addresses	v.v.v.v:vport	virtual (local) address\n"
	       "\t		d.d.d.d:dport	real server (destination) "
		"address\n", DEF_SCHED);
	exit(1);
}


void fail(int err, char *text) {
	printf("%s\n",text);
	exit(err);
}


void list_forwarding_exit(void)
{
    char buffer[1024];

    FILE *handle;
    handle = fopen ("/proc/net/ip_masq/vs", "r");
    if (!handle) {
	    printf ("Could not open /proc/net/ip_masq/vs\n");
	    printf ("Are you sure that Virtual Server is supported by the kernel?\n");
	    exit (1);
    }
    while (!feof (handle)) {
	    if (fgets (buffer, sizeof(buffer), handle)) printf ("%s", buffer);
    }
    fclose (handle);
    exit (0);
}
