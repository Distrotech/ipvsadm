CC      = gcc
CFLAGS	= -Wall -Wunused -g -O2
SBIN    = /sbin
MAN     = /usr/man/man8
INSTALL = install
INCLUDE = -I/usr/src/linux/include

all:            ipvsadm

ipvsadm:	ipvsadm.c
		$(CC) $(CFLAGS) $(INCLUDE) -o ipvsadm ipvsadm.c

install:        ipvsadm
		strip ipvsadm
		$(INSTALL) -m 0755 -o root -g root ipvsadm $(SBIN)
		$(INSTALL) -m 0755 -o root -g root ipvsadm.8 $(MAN)

clean:
		rm -f ipvsadm *.o core *~

