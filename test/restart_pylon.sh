#!/bin/sh

PYLON_STATUS=`/sbin/service pylon status | grep uptime | wc -l` 
PYLON_LOG_SIZE=`/usr/bin/stat /tmp/pylon.log --printf="%s"`
[ $PYLON_STATUS = 0 ] && /sbin/service pylon start
[ $PYLON_LOG_SIZE -gt 500000000 ] && > /tmp/pylon.log

