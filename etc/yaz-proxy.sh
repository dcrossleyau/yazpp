#!/bin/sh
# $Id: yaz-proxy.sh,v 1.2 2003-10-24 10:33:01 adam Exp $
# YAZ proxy start/stop init.d script.
#
PATH=/usr/local/bin:/bin:/usr/bin
export PATH

# Proxy CWD is here. Should be writable by it.
DIR=/var/yaz-proxy
# Proxy Path
DAEMON="/usr/local/bin/yaz-proxy"

# Proxy PIDFILE. Must be writable by it.
PIDFILE="/var/run/yaz-proxy.pid"

# Log file
LOGFILE=/var/log/yaz-proxy.log

# Port
PORT=9000

# Run as this user. Set to empty to keep uid as is
RUNAS=nobody

# Extra args . Config file _WITH_ option
ARGS="-c config.xml"

if test -n "RUNAS"; then
	ARGS="-u $RUNAS $ARGS"
fi

# Name, Description (not essential)
NAME=yaz-proxy
DESC="YAZ proxy"

test -d $DIR || exit 0
test -f $DAEMON || exit 0

set -e

case "$1" in
  start)
	printf "%s" "Starting $DESC: "
	cd $DIR
	$DAEMON -l $LOGFILE -p $PIDFILE $ARGS @:$PORT &
	echo "$NAME."
	;;
  stop)
	printf "%s" "Stopping $DESC: "

	if test -f $PIDFILE; then
		kill `cat $PIDFILE`
		rm -f $PIDFILE
		echo "$NAME."
	else
		echo "No PID $PIDFILE"
	fi
	;;
  reload)
	if test -f $PIDFILE; then
		kill -HUP `cat $PIDFILE`
	fi
  ;;
  restart|force-reload)
	printf "%s" "Restarting $DESC: "
	if test -f $PIDFILE; then
		kill `cat $PIDFILE`
		rm -f $PIDFILE
	fi
	sleep 1
	cd $DIR
	$DAEMON -l $LOGFILE -p $PIDFILE $ARGS @:$PORT &
	echo "$NAME."
	;;
  *)
	N=/etc/init.d/$NAME
	# echo "Usage: $N {start|stop|restart|reload|force-reload}" >&2
	echo "Usage: $N {start|stop|restart|force-reload}" >&2
	exit 1
	;;
esac

exit 0
