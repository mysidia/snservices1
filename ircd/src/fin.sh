#!/bin/sh
if [ -f "Makefile.err" ] ; then
more Makefile.err
echo "[Press enter to continue]"
read 'foo'
rm -f clock
exit
fi

if [ -f "clock" ] ; then
START="`cat clock |head -1`"
EFILES="`cat clock |wc -l`"
#IFILES="`ls *.c -lss | wc -l`"
rm -f clock
STOP="`echo '' | awk '{print systime()}'`"
#let EFILES=EFILES-1
##let IFILES=IFILES
#let END=STOP-START
echo "[$(($EFILES-1))] files compiled in $(($STOP-$START)) seconds "
fi
