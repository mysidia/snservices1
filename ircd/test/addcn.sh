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
cat <<EOF>>$1

# $0 $1 $2 $3
C:127.0.0.1:b2357:$2:$3:100
N:127.0.0.1:b2357:$2:$3:100
H:*::$2
EOF

