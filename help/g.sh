#!/bin/sh

cd chanserv

for i in *.xml ; do
   TARFILE="`echo $i |sed s/'.xml'/'.html'/`"
   ../hparse2 $i *.xml > $TARFILE
done
