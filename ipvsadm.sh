#!/bin/sh
#
# Startup script handle the initialisation of LVS
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
# config: /etc/ipvsadm.rules


# Exit silently if there is no configuration file
if [ -f "/etc/sysconfig/ipvsadm"  ]; then
  IPVSADM_CONFIG="/etc/sysconfig/ipvsadm"
elif [ -f "/etc/ipvsadm.rules"  ]; then
  IPVSADM_CONFIG="/etc/ipvsadm.rules"
else
  exit 0
fi

# Use the funtions provided by Red Hat or use our own
if [ -f /etc/rc.d/init.d/functions ]
then
  . /etc/rc.d/init.d/functions
else
  function action {
    echo "$1"
    shift
    $@
  }
  function success {
    echo -n "Success"
  }
  function Failure {
    echo -n "Failed"
  }
fi

# Check for ipvsadm in both /sbin and /usr/sbin
# The default install puts it in /sbin, as it is analogos to commands such
# as route and ipchains that live in /sbin.  Some vendors, most notibly 
# Red Hat insist on moving it to /usr/sbin
if [ ! -x /sbin/ipvsadm -a  ! -x /usr/sbin/ipvsadm ]; then
    exit 0
fi

case "$1" in
  start)
    # If we don't clear these first, we might be adding to
    #  pre-existing rules.
    action "Clearing the current IPVS table:" ipvsadm -C
    echo -n "Applying IPVS configuration: "
      grep -v "^#" $IPVSADM_CONFIG | ipvsadm-restore && \
      success "Applying IPVS configuration" || \
      failure "Applying IPVS configuration"
    echo
    touch /var/lock/subsys/ipvsadm
  ;;

  stop)
        action "Clearing the current IPVS table:" ipvsadm -C
	rm -f /var/lock/subsys/ipvsadm
	;;

  reload|reload-force|restart)
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
	echo "Usage: ipvsadm
	  {start|stop|restart|status|panic|save|reload|reload-force}"
	exit 1
esac

exit 0

