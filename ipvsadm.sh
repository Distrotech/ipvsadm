#!/bin/sh
#
# Startup script handle the initialisation of IPVS
#
# chkconfig: - 08 92
#
# description: Initialise the Linux Virtual Server
#              http://www.linuxvirtualserver.org/
#
# Script Author: Horms <horms@vergenet.net>
#
# Based on init script for ipchains by Joshua Jensen <joshua@redhat.com>
#
# config: /etc/sysconfig/ipvsadm


IPVSADM_CONFIG=/etc/sysconfig/ipvsadm

# Exit silently if there is no configuration file
if [ ! -f $IPVSADM_CONFIG ]; then
  exit
fi

# Use the funtions provided by Red Hat
# This should be made distribution agnostic
. /etc/rc.d/init.d/functions

# Check for ipvsadm in both /sbin and /usr/sbin
# The default install puts it in /sbin, as it is analogos to commands such
# as route and ipchains that live in /sbin.  Some vendors, most notibly 
# Red Hat insist on movint it to /usr/sbin
if [ ! -x /sbin/ipvsadm -a  ! -x /usr/sbin/ipvsadm ]; then
    exit 0
fi

case "$1" in
  start)
    # If we don't clear these first, we might be adding to
    #  pre-existing rules.
    action "Clearing the current IPVS table:" ipvsadm -C
    echo -n "Applying IPVS configuration: "
      grep -v "^#" $IPVSADM_CONFIG | ipvsadm-restore -p -f && \
      success "Applying IPVS configuration" || \
      failure "Applying IPVS configuration"
    echo
    touch /var/lock/subsys/ipvsadm
  ;;

  stop)
        action "Clearing the current IPVS table:" ipvsadm -C
	rm -f /var/lock/subsys/ipvsadm
	;;

  restart)
	#Start should flush everything
	$0 start
	;;

  panic)
	# I'm not sure what panic does but in the case of IPVS	
        # it makes sense just to clear everything
        action "Clearing the current IPVS table:" ipvsadm -C
	;;

  status)
	ipvsadm -L -n
	;;

  save)
	echo -n "Saving IPVS table to $IPVSADM_CONFIG: "
	ipvsadm-save > $IPVSADM_CONFIG  2>/dev/null && \
	  success "Saving IPVS table to $IPVSADM_CONFIG" || \
	  failure "Saving IPVS table to $IPVSADM_CONFIG"
        echo
	;;

  *)
	echo "Usage: $0 {start|stop|restart|status|panic|save}"
	exit 1
esac

exit 0

