#
#      ipvsadm - IP Virtual Server ADMinistration program
#
#      Version: $Id$
#
#      Authors: Wensong Zhang <wensong@gnuchina.org>
#               Peter Kese <peter.kese@ijs.si>
#
#      This file:
#
#      ChangeLog
#
#      Wensong        :   Modified the Makefile and the spec files so
#                     :   that rpms can be created with ipvsadm alone
#      P.Copeland     :   Modified the Makefile and the spec files so
#                     :   that it is possible to create rpms on the fly
#                     :   using 'make rpms'
#                     :   Also added NAME, VERSION and RELEASE numbers to
#                     :   the Makefile
#      Horms          :   Updated to add config_stream.c dynamic_array.c
#                     :   Added autodetection of libpot
#                     :   Added BUILD_ROOT support
#      Wensong        :   Changed the OBJS according to detection
#      Horms          :   Moved ipvsadm back into /sbin where it belongs
#                         as it is more or less analogous to both route 
#                         and ipchains both of which reside in /sbin.
#                         Added rpm target whose only dependancy is 
#                         the rpms target
#

NAME	= ipvsadm
VERSION	= 1.12
RELEASE	= 1

CC	= gcc
CFLAGS	= -Wall -Wunused -g -O2
SBIN    = $(BUILD_ROOT)/sbin
MAN     = $(BUILD_ROOT)/usr/man/man8
MKDIR   = mkdir
INSTALL = install
INCLUDE = -I/usr/src/linux/include
LIB_SEARCH = /lib /usr/lib /usr/local/lib

# Where to install INIT scripts
# Will only install files here if these directories already exist
# as if the directories don't exist then the system is unlikely to
# use the files
INIT = $(BUILD_ROOT)/etc/rc.d/init.d

#####################################
# No servicable parts below this line

POPT_LIB = $(shell for i in $(LIB_SEARCH); do \
  if [ -f $$i/libpopt.a ]; then \
    if nm $$i/libpopt.a | fgrep -q poptGetContext; then \
        echo "-L$$i -lpopt"; \
    fi; \
  fi; \
done)

ifneq (,$(POPT_LIB))
POPT_DEFINE = -DHAVE_POPT
OBJS = config_stream.o dynamic_array.o ipvsadm.o
else
OBJS = ipvsadm.o
endif

LIBS = $(POPT_LIB)
DEFINES = $(POPT_DEFINE)

.PHONY = all clean install

all:            ipvsadm

ipvsadm:	$(OBJS)
		$(CC) $(CFLAGS) -o ipvsadm $(OBJS) $(LIBS)

install:        ipvsadm
		strip ipvsadm
		if [ ! -d $(SBIN) ]; then $(MKDIR) -p $(SBIN); fi
		$(INSTALL) -m 0755 ipvsadm $(SBIN)
		$(INSTALL) -m 0755 ipvsadm-save $(SBIN)
		$(INSTALL) -m 0755 ipvsadm-restore $(SBIN)
		if [ ! -d $(MAN) ]; then $(MKDIR) -p $(MAN); fi
		$(INSTALL) -m 0644 ipvsadm.8 $(MAN)
		if [ -d $(INIT) ]; then \
		  $(INSTALL) -m 0755 ipvsadm.sh $(INIT)/ipvsadm ;\
		fi

clean:
		rm -f ipvsadm *.o core *~ $(NAME).spec \
			$(NAME)-$(VERSION).tar.gz

dist:		clean
		sed -e "s/@@VERSION@@/$(VERSION)/g" \
                    -e "s/@@RELEASE@@/$(RELEASE)/g" \
                    < ipvsadm.spec.in > ipvsadm.spec
		( cd .. ; tar czvf $(NAME)-$(VERSION).tar.gz \
			--exclude CVS \
			--exclude $(NAME)-$(VERSION).tar.gz \
			ipvsadm ; \
			mv $(NAME)-$(VERSION).tar.gz ipvsadm )

rpm:		rpms

rpms:		dist
		cp $(NAME)-$(VERSION).tar.gz /usr/src/redhat/SOURCES/
		cp $(NAME).spec /usr/src/redhat/SPECS/
		(cd /usr/src/redhat/SPECS/ ; rpm -ba $(NAME).spec)

%.o:	%.c
	$(CC) $(CFLAGS) $(INCLUDE) $(DEFINES) -c $<
