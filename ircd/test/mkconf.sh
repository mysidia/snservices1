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

cat <<EOF >./$1
# Base Automatically generated 

# $0 $1 $2 $3
M:$2:127.0.0.1:Test 1:$3
A:Fooadmin:`whoami`:`whoami`@`hostname`

Y:100:50:45:1:500000
Y:150:55:0:0:500000
Y:160:55:50:1:500000
Y:200:60:0:0:500000
Y:210:60:55:1:500000
Y:1:120:0:1000:100000
Y:10:360:0:20:300000

I:*::*::1:!sockscheck

EOF


