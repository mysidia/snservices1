#!/bin/sh
#
# Copyright (c) 2001 James Hess
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the authors nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

SERVICESHOME="/home/sorserv/installed/services"
AWK=/usr/bin/awk
WC=/usr/bin/wc
PS=/bin/ps
LS=/bin/ls
TAIL=/usr/bin/tail
GREP=/usr/bin/grep
EGREP=/usr/bin/egrep

cd $SERVICESHOME

getspid () {
SPID="`$PS axwu | $AWK '{if ($1=="sorserv"&&index($11,"services")>0){print $2}}'`"

if [ -n "$SPID" ] ; then
   echo "Services are already running."
   exit 1
fi

}

checkdb () {
if [ ! -r $SERVICESHOME/$1/$2 ] ; then
    echo "$3 :  $1/$2 is missing!"
    DBMISS=1
else
   X="`$WC -c $SERVICESHOME/$1/$2 | $AWK '{print $1}'`"

   if [ $X -le $4 ] ; then
       echo "Warning: $2 is less than $4 bytes."
       echo "$3: "
   fi

   if [ "$4" = "1" ] ; then
       X="`$TAIL -1 $SERVIESHOME/$1/$2 |egrep ^done`"
       if [ -z "$X" ] ; then
           DBMISS=1
           echo "$3:   Database is not formatted properly! Missing trailing 'done'!"
       fi
   fi
   $LS -l -- $SERVICESHOME/$1/$2
fi
}

if [ "`whoami`" != "sorserv" ] ; then
   echo "Only sorserv can run this script."
   exit 1
fi

getspid

cat <<EOF

This script is configured for restarting the copy of services found
in : $SERVICESHOME

Please indicate why services need to be restarted by choosing
one of the following:

1. The services machine rebooted or services were manually shut down
   but services themselves did not crash.
2. Services crashed.
3. You changed your mind and don't want to restart services

EOF
echo -n ">> "
read 'foo'

case "$foo" in
*crash*)
foo='1'
;;
1)
foo='1'
;;
2)
foo='2'
;;
*reboot*)
foo='2'
;;
*machine*)
foo='2'
;;
*quit*)
foo='3'
;;
*abort*)
foo='3'
;;
3)
foo='3'
;;
*)
  echo "Invalid option."
  exit 1
;;
esac

if [ "$foo" = "3" ] ; then
echo "Aborted"
exit 0
fi

getspid

if [ "$foo" = "2" ] ; then
echo "Renaming core and logfiles..."
./corename.sh
fi

getspid

echo "Performing rudimentary checks on services' databases..."
DBMISS=0
DBWARN=0

checkdb nickserv nickserv.db Nicknames 1024000 1
checkdb chanserv chanserv.db Channels 614400 1
checkdb operserv trigger.db Triggers 50 1
checkdb operserv akill.db Autokills  1024 0
checkdb memoserv memoserv.db Memos 153600 1
if [ "$DBMISS" -ne 0 ] ; then
echo ">> A fatal error occured: database integrity problem."
exit 1
fi
if [ "$DBWARN" -ne 0 ] ; then
echo "Are you sure you want to proceed?"
echo -n "[Y/n] --> "
read 'foo'
case "$foo" in
[yY])
foo='y'
;;
[yY][eE][sS])
foo='y'
;;
*)
echo "Ok, aborting."
exit 1
;;
esac
fi

getspid

./services &
