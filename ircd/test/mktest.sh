#!/bin/sh
#
#   Internet Relay Chat Daemon Test Scripts, src/s_service.c
#   Copyright (C) 1998 Mysidia
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 1, or (at your option)
#   any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

n=3
p=9099
CONFS=''

if [ ! -x ../src/ircd ] ; then
	  echo "Ircd need first be compiled"
	  exit
fi

i=1

while [ $i -le $n ] ; do
 if [ -n "$CONFS" ] ; then
	CONFS="$CONFS test$i.conf"
 else
	 CONFS="test$i.conf"
 fi

sh ./mkconf.sh test$i.conf test$i.server.irc $((p+i))
j=1

 while [ $j -le $n ] ; do
      sh ./addcn.sh test$i.conf test$j.server.irc $((p+j))
      j=$((j+1))
 done
i=$((i+1))
done

cp ../src/ircd ./ircd.testbin
if [ $? -ne 0 ] ; then
	echo "Copy of ircd binary failed"
	exit 1
fi

cat <<EOF >spawn.sh

echo -n "Spawning servers..."
a=1
for i in $CONFS ; do
	echo -n \$a
	./ircd.testbin -f `pwd`/\$i
	a=\$((a+1))
done
echo ""
echo "Test servers should be running..."
ps axwu |fgrep 'ircd.testbin' |fgrep -v fgrep
echo ""
EOF
