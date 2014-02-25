#!/bin/sh

LOCKFILE=/tmp/pylon_restart.lock
[ -f $LOCKFILE ] && exit

touch $LOCKFILE
TODAY=`date "+%Y%m%d%H%M"`
PYLON_STATUS=`/sbin/service pylon status | grep uptime | wc -l` 
PYLON_LOG_SIZE=`/usr/bin/stat /tmp/pylon.log --printf="%s"`
if [ $PYLON_STATUS = 0 ]; then
    ps auxww | grep pylon > $LOCKFILE
    /sbin/service pylon stop
    tail -n 100 /tmp/pylon.log >> $LOCKFILE
    cat $LOCKFILE | mail -s "Restarting pylon" pgillan@ffn.com
    sleep 2
    /bin/gzip -c /tmp/pylon.log > /tmp/pylon-$TODAY.log.gz
    /sbin/service pylon start
fi
[ $PYLON_LOG_SIZE -gt 250000000 ] && > /tmp/pylon.log
rm -f $LOCKFILE

