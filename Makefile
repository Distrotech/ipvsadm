#
#      ipvsadm - IP Virtual Server ADMinistration program
#
#      Version: $Id$
#
#      Authors: Wensong Zhang <wensong@iinchina.net>
#               Peter Kese <peter.kese@ijs.si>
#
#      This file:
#
#      ChangeLog
#      Horms          :   Updated to add config_stream.c dynamic_array.c
#                     :   Added autodetection of libpot
#                     :   Added BUILD_ROOT support
#      Wensong        :   Changed the OBJS according to detection
#

CC      = gcc
CFLAGS	= -Wall -Wunused -g -O2
SBIN    = $(BUILD_ROOT)/sbin
MAN     = $(BUILD_ROOT)/usr/man/man8
INSTALL = install
INCLUDE = -I/usr/src/linux/include
LIB_SEARCH = /lib /usr/lib /usr/local/lib


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

.PHONY = all clean

all:            ipvsadm

ipvsadm:	$(OBJS)
		echo $(LIBS)
		$(CC) $(CFLAGS) -o ipvsadm $(OBJS) $(LIBS)

install:        ipvsadm
		strip ipvsadm
		$(INSTALL) -m 0755 -o root -g root ipvsadm $(SBIN)
		$(INSTALL) -m 0644 -o root -g root ipvsadm.8 $(MAN)

clean:
		rm -f ipvsadm *.o core *~

%.o:	%.c
	$(CC) $(CFLAGS) $(INCLUDE) $(DEFINES) -c $<
