#!/bin/sh
DATE=/bin/date

N1="`$DATE +%y%m%d`"
N2="`$DATE +%y%m%d-%k`"
N3="`$DATE +%y%m%d-%k-%M`$$"
COREFILE="services.core"
CORELOGFILE="core.log"
BINFILE="services"

if [ -z "$N1" ] ; then
 echo ERROR
 exit 1
fi

if [ ! -w $COREFILE ]  ; then
    echo "$COREFILE not found."
    exit 0
fi

if [ ! -w $CORELOGFILE ]  ; then
    echo "$CORELOGFILE not found."
    exit 0
fi

if [ -d "$N1" ] ; then
   N1="$N2"
fi

if [ -d "$N1" ] ; then
   N1="$3"
fi

CRASHDIR="crash.$N1"

mkdir $CRASHDIR
if [ $? -ne 0 ] ; then
   echo "Error creating $CRASHDIR directory"
   exit 1
fi

echo mv -- $COREFILE $CRASHDIR
mv -- $COREFILE $CRASHDIR
if [ $? -ne 0 ] ; then
   echo "Error moving $COREFILE into $CRASHDIR"
fi

echo mv -- $CORELOGFILE $CRASHDIR
mv -- $CORELOGFILE $CRASHDIR
if [ $? -ne 0 ] ; then
   echo "Error moving $CORELOGFILE into $CRASHDIR"
fi

echo ln -- $BINFILE $CRASHDIR/$BINFILE
ln -- $BINFILE $CRASHDIR/$BINFILE
if [ $? -ne 0 ] ; then
   echo "Error linking $BINFILE into $CRASHDIR"
fi

