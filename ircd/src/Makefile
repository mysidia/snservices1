#************************************************************************
#*   IRC - Internet Relay Chat, src/Makefile
#*   Copyright (C) 1990 Jarkko Oikarinen
#*
#*   This program is free software; you can redistribute it and/or modify
#*   it under the terms of the GNU General Public License as published by
#*   the Free Software Foundation; either version 1, or (at your option)
#*   any later version.
#*
#*   This program is distributed in the hope that it will be useful,
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#*   GNU General Public License for more details.
#*
#*   You should have received a copy of the GNU General Public License
#*   along with this program; if not, write to the Free Software
#*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#*/
DIRCHG=(cd ../;make)
CC=danger will robinson

LINTFLAGS=-hba

OBJS=bsd.o dbuf.o packet.o send.o match.o parse.o support.o channel.o \
	class.o hash.o ircd.o list.o res.o s_bsd.o s_conf.o \
	s_debug.o s_err.o s_misc.o s_numeric.o s_serv.o s_user.o s_service.o \
	whowas.o userload.o md5.o version.o network.o socket.o socket_addr.o \
	snprintf.o memory.o string.o $(RES) $(STRTOUL) $(GETADDRINFO) $(SOCKET)

SRC=$(OBJS:%.o=%.c)

#MAKE = make 'CFLAGS=${CFLAGS}' 'CC=${CC}' 'IRCDLIBS=${IRCDLIBS}' \
#	    'LDFLAGS=${LDFLAGS}' 'IRCDMODE=${IRCDMODE}' \
#            'DESCRYPT=${DESCRYPT}'

INCLUDES = ../include/struct.h ../include/config.h ../include/options.h \
	../include/sys.h ../include/common.h ../include/patchlevel.h \
	../include/h.h ../include/numeric.h ../include/msg.h


all:
	${DIRCHG}
build: ircd chkconf
version.c:
	$(SHELL) version.c.SH

version.o: version.c ../include/patchlevel.h
	$(CC) $(CFLAGS) $(DFLAGS) -c version.c

# XXX on RH6 i get -lresolv and -lcrypt linked twice
# this is b/c of $(IRCDLIBS) $(DESCRYPT)

ircd: $(OBJS) 
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(IRCDLIBS) $(DESCRYPT) -o ircd
	@chmod $(IRCDMODE) ircd

chkconf: ../include/struct.h ../include/config.h ../include/options.h ../include/sys.h \
	 ../include/common.h chkconf.c memory.o
	$(CC) -ggdb $(CFLAGS) -DCR_CHKCONF -o chkmatch.o -c match.c
	$(CC) $(DFLAGS) $(CFLAGS) chkconf.c chkmatch.o memory.o string.o \
	$(LDFLAGS) $(IRCDLIBS) -o chkconf

saber: $(SRC)
	#load -I../include $(SRC) version.c $(IRCDLIBS)

lint:
	lint $(LINTFLAGS) -I../include $(SRC) | egrep -v 'sendto_|debug'

parse.o: parse.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS) -o parse.o -c parse.c
bsd.o: bsd.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS) -o bsd.o -c bsd.c
dbuf.o: dbuf.c $(INCLUDES) ../include/dbuf.h
	$(CC) $(XCFLAGS) $(CFLAGS) -o dbuf.o -c dbuf.c
packet.o: packet.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS) -o packet.o -c packet.c
send.o: send.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS) -o send.o -c send.c
match.o: match.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS) -o match.o -c match.c
support.o: support.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS) -o support.o -c support.c
userload.o: userload.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS) -o userload.o -c userload.c

clean:
	$(RM) -f *.o *~ core ircd version.c chkconf

cleandir: clean

depend:
	makedepend -Y -I${INCLUDEDIR} ${SRC}

channel.o: channel.c $(INCLUDES) ../include/channel.h
	$(CC) $(CFLAGS) -c channel.c

class.o: class.c $(INCLUDES) ../include/class.h
	$(CC) $(CFLAGS) -c class.c

ircd.o: ircd.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c ircd.c

list.o: list.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c list.c

res.o: res.c ../include/res.h $(INCLUDES)
	$(CC) $(CFLAGS) -c res.c

s_bsd.o: s_bsd.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_bsd.c

s_conf.o: s_conf.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_conf.c

s_debug.o: ../include/sys.h s_debug.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_debug.c

s_err.o: ../include/msg.h s_err.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_err.c

s_misc.o: s_misc.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_misc.c

s_user.o: s_user.c $(INCLUDES) \
	  ../include/dbuf.h ../include/channel.h ../include/whowas.h
	$(CC) $(CFLAGS) -c s_user.c

s_service.o: s_service.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_service.c

s_serv.o: s_serv.c $(INCLUDES) \
	  ../include/dbuf.h ../include/whowas.h
	$(CC) $(CFLAGS) -c s_serv.c

s_numeric.o: s_numeric.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_numeric.c

whowas.o: whowas.c ../include/dbuf.h ../include/whowas.h $(INCLUDES)
	$(CC) $(CFLAGS) -c whowas.c

hash.o: hash.c ../include/hash.h $(INCLUDES)
	$(CC) $(CFLAGS) -c hash.c

md5.o: md5.c $(INCLUDES)
	$(CC) $(CFLAGS) -c md5.c

# DO NOT DELETE THIS LINE -- make depend depends on it.
