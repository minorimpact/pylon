#!/bin/sh

LOCKFILE=/tmp/pylon_restart.lock
[ -f $LOCKFILE ] && exit

touch $LOCKFILE
HOSTNAME=`hostname -s`
TODAY=`date "+%Y%m%d%H%M"`
PYLON_STATUS=`/sbin/service pylon status | grep uptime | wc -l` 
PYLON_LOG_SIZE=0
[ -f /tmp/pylon.log ] && PYLON_LOG_SIZE=`/usr/bin/stat /tmp/pylon.log --printf="%s"`
if [ $PYLON_STATUS = 0 ]; then
    ps auxww | grep pylon > $LOCKFILE
    /sbin/service pylon stop
    tail -n 100 /tmp/pylon.log >> $LOCKFILE
    cat $LOCKFILE | mail -s "Restarting pylon on $HOSTNAME" pgillan@ffn.com
    sleep 2
    [ -f /tmp/pylon.log ] && /bin/gzip -c /tmp/pylon.log > /tmp/pylon-$TODAY.log.gz
    rm -f /tmp/pylon.log
    /sbin/service pylon start
fi
[ $PYLON_LOG_SIZE -gt 2000000000 ] && > /tmp/pylon.log
rm -f $LOCKFILE

