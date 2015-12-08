#!/bin/sh

device="ppsim"
mode="664"

# invoke insmod with all arguments we got
# and use a pathname, as newer modutils don't look in . by default
/sbin/insmod ./simulator.ko $* || exit 1

echo module inserted

# remove stale node
rm -f /dev/${device}0

major=`awk "\\$2==\"$module\" {print \\$1}" /proc/devices`

echo major:$major

mknod /dev/${device}0 c $major 0
