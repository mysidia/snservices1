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
	class.o hash.o ircd.o list.o res.o s_auth.o s_bsd.o s_conf.o \
	s_debug.o s_err.o s_misc.o s_numeric.o s_serv.o s_user.o s_service.o \
	whowas.o userload.o crule.o md5.o help.o version.o s_sock.o \
	snprintf.o memory.o string.o $(RES) $(STRTOUL)

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
	 ../include/common.h crule.c chkconf.c memory.o
	$(CC) $(DFLAGS) $(CFLAGS) -DCR_CHKCONF -o chkcrule.o -c crule.c
	$(CC) -ggdb $(CFLAGS) -DCR_CHKCONF -o chkmatch.o -c match.c
	$(CC) $(DFLAGS) $(CFLAGS) chkconf.c chkmatch.o chkcrule.o memory.o string.o \
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
help.o: help.c $(INCLUDES)

#install: all
#	-if [ ! -d ${IRCDDIR} -a ! -f ${IRCDDIR} ] ; then \
#		mkdir ${IRCDDIR}; \
#	fi
#	../bsdinstall -m ${IRCDMODE} ircd ${BINDIR}
#	../bsdinstall -m 700 chkconf ${BINDIR}
#	$(CP) ../doc/example.conf ${IRCDDIR}
#	$(TOUCH) ${IRCDDIR}/ircd.motd
#	$(RM) -f ${IRCDDIR}/ircd.m4
#	$(TOUCH) ${IRCDDIR}/ircd.m4
#	chmod +x buildm4
#	./buildm4 ${IRCDDIR}

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

s_auth.o: s_auth.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_auth.c

s_sock.o: s_sock.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_sock.c

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

crule.o: crule.c $(INCLUDES)
	$(CC) $(CFLAGS) -c crule.c

help.o: help.c $(INCLUDES)
	$(CC) $(CFLAGS) -c help.c

md5.o: md5.c $(INCLUDES)
	$(CC) $(CFLAGS) -c md5.c

# DO NOT DELETE THIS LINE -- make depend depends on it.

bsd.o: ../include/struct.h ../include/config.h ../include/setup.h
bsd.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
bsd.o: ../include/sys.h ../include/snprintf.h ../include/class.h
bsd.o: ../include/dbuf.h ../include/h.h ../include/ircd/memory.h
bsd.o: ../include/ircd/send.h
dbuf.o: ../include/struct.h ../include/config.h ../include/setup.h
dbuf.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
dbuf.o: ../include/sys.h ../include/snprintf.h ../include/class.h
dbuf.o: ../include/dbuf.h ../include/h.h ../include/ircd/memory.h
packet.o: ../include/struct.h ../include/config.h ../include/setup.h
packet.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
packet.o: ../include/sys.h ../include/snprintf.h ../include/class.h
packet.o: ../include/dbuf.h ../include/msg.h ../include/h.h
packet.o: ../include/ircd/memory.h
send.o: ../include/struct.h ../include/config.h ../include/setup.h
send.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
send.o: ../include/sys.h ../include/snprintf.h ../include/class.h
send.o: ../include/dbuf.h ../include/h.h ../include/ircd/memory.h
send.o: ../include/ircd/send.h ../include/ircd/string.h
match.o: ../include/struct.h ../include/config.h ../include/setup.h
match.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
match.o: ../include/sys.h ../include/snprintf.h ../include/class.h
match.o: ../include/dbuf.h ../include/ircd/match.h ../include/ircd/string.h
parse.o: ../include/struct.h ../include/config.h ../include/setup.h
parse.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
parse.o: ../include/sys.h ../include/snprintf.h ../include/class.h
parse.o: ../include/dbuf.h ../include/msg.h ../include/numeric.h
parse.o: ../include/h.h ../include/ircd/memory.h ../include/ircd/match.h
parse.o: ../include/ircd/send.h ../include/ircd/string.h
support.o: ../include/config.h ../include/setup.h ../include/options.h
support.o: ../include/struct.h ../include/common.h ../include/ircd/cdefs.h
support.o: ../include/sys.h ../include/snprintf.h ../include/class.h
support.o: ../include/dbuf.h
channel.o: ../include/struct.h ../include/config.h ../include/setup.h
channel.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
channel.o: ../include/sys.h ../include/snprintf.h ../include/class.h
channel.o: ../include/dbuf.h ../include/numeric.h ../include/channel.h
channel.o: ../include/hash.h ../include/msg.h ../include/h.h
channel.o: ../include/ircd/memory.h ../include/ircd/channel.h
channel.o: ../include/ircd/match.h ../include/ircd/send.h
channel.o: ../include/ircd/string.h
class.o: ../include/struct.h ../include/config.h ../include/setup.h
class.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
class.o: ../include/sys.h ../include/snprintf.h ../include/class.h
class.o: ../include/dbuf.h ../include/numeric.h ../include/h.h
class.o: ../include/ircd/memory.h ../include/ircd/send.h
hash.o: ../include/struct.h ../include/config.h ../include/setup.h
hash.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
hash.o: ../include/sys.h ../include/snprintf.h ../include/class.h
hash.o: ../include/dbuf.h ../include/hash.h ../include/numeric.h
hash.o: ../include/h.h ../include/ircd/memory.h ../include/msg.h
hash.o: ../include/ircd/send.h ../include/ircd/string.h
ircd.o: ../include/struct.h ../include/config.h ../include/setup.h
ircd.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
ircd.o: ../include/sys.h ../include/snprintf.h ../include/class.h
ircd.o: ../include/dbuf.h ../include/numeric.h ../include/userload.h
ircd.o: ../include/h.h ../include/ircd/memory.h ../include/ircd/send.h
list.o: ../include/struct.h ../include/config.h ../include/setup.h
list.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
list.o: ../include/sys.h ../include/snprintf.h ../include/class.h
list.o: ../include/dbuf.h ../include/h.h ../include/ircd/memory.h
list.o: ../include/numeric.h ../include/ircd/send.h
res.o: ../include/struct.h ../include/config.h ../include/setup.h
res.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
res.o: ../include/sys.h ../include/snprintf.h ../include/class.h
res.o: ../include/dbuf.h ../include/res.h ../include/numeric.h ../include/h.h
res.o: ../include/ircd/memory.h ../include/nameser.h ../include/resolv.h
res.o: ../include/ircd/send.h ../include/ircd/string.h ../include/ircd/res.h
s_auth.o: ../include/struct.h ../include/config.h ../include/setup.h
s_auth.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_auth.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_auth.o: ../include/dbuf.h ../include/res.h ../include/numeric.h
s_auth.o: ../include/patchlevel.h ../include/sock.h ../include/h.h
s_auth.o: ../include/ircd/memory.h ../include/ircd/send.h
s_auth.o: ../include/ircd/string.h
s_bsd.o: ../include/struct.h ../include/config.h ../include/setup.h
s_bsd.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_bsd.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_bsd.o: ../include/dbuf.h ../include/res.h ../include/numeric.h
s_bsd.o: ../include/patchlevel.h ../include/inet.h ../include/nameser.h
s_bsd.o: ../include/resolv.h ../include/sock.h ../include/h.h
s_bsd.o: ../include/ircd/memory.h ../include/ircd/res.h
s_bsd.o: ../include/ircd/send.h ../include/ircd/string.h
s_conf.o: ../include/struct.h ../include/config.h ../include/setup.h
s_conf.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_conf.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_conf.o: ../include/dbuf.h ../include/numeric.h ../include/h.h
s_conf.o: ../include/ircd/memory.h ../include/msg.h ../include/ircd/match.h
s_conf.o: ../include/ircd/send.h ../include/ircd/string.h
s_debug.o: ../include/struct.h ../include/config.h ../include/setup.h
s_debug.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_debug.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_debug.o: ../include/dbuf.h ../include/ircd/send.h ../include/numeric.h
s_debug.o: ../include/whowas.h ../include/hash.h ../include/h.h
s_debug.o: ../include/ircd/memory.h
s_err.o: ../include/struct.h ../include/config.h ../include/setup.h
s_err.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_err.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_err.o: ../include/dbuf.h ../include/numeric.h
s_misc.o: ../include/struct.h ../include/config.h ../include/setup.h
s_misc.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_misc.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_misc.o: ../include/dbuf.h ../include/numeric.h ../include/userload.h
s_misc.o: ../include/h.h ../include/ircd/memory.h ../include/msg.h
s_misc.o: ../include/ircd/send.h ../include/ircd/string.h
s_numeric.o: ../include/struct.h ../include/config.h ../include/setup.h
s_numeric.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_numeric.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_numeric.o: ../include/dbuf.h ../include/numeric.h ../include/h.h
s_numeric.o: ../include/ircd/memory.h ../include/ircd/send.h
s_serv.o: ../include/struct.h ../include/config.h ../include/setup.h
s_serv.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_serv.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_serv.o: ../include/dbuf.h ../include/numeric.h ../include/msg.h
s_serv.o: ../include/channel.h ../include/userload.h ../include/h.h
s_serv.o: ../include/ircd/memory.h ../include/ircd/match.h
s_serv.o: ../include/ircd/send.h ../include/ircd/string.h
s_user.o: ../include/struct.h ../include/config.h ../include/setup.h
s_user.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_user.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_user.o: ../include/dbuf.h ../include/numeric.h ../include/msg.h
s_user.o: ../include/channel.h ../include/userload.h ../include/h.h
s_user.o: ../include/ircd/memory.h ../include/ircd/match.h
s_user.o: ../include/ircd/send.h ../include/ircd/string.h
s_user.o: ../include/ircd/md5.h
s_service.o: ../include/struct.h ../include/config.h ../include/setup.h
s_service.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_service.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_service.o: ../include/dbuf.h ../include/numeric.h ../include/msg.h
s_service.o: ../include/channel.h ../include/userload.h ../include/h.h
s_service.o: ../include/ircd/memory.h ../include/ircd/send.h
s_service.o: ../include/ircd/string.h
whowas.o: ../include/struct.h ../include/config.h ../include/setup.h
whowas.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
whowas.o: ../include/sys.h ../include/snprintf.h ../include/class.h
whowas.o: ../include/dbuf.h ../include/numeric.h ../include/whowas.h
whowas.o: ../include/h.h ../include/ircd/memory.h ../include/ircd/send.h
whowas.o: ../include/ircd/string.h
userload.o: ../include/struct.h ../include/config.h ../include/setup.h
userload.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
userload.o: ../include/sys.h ../include/snprintf.h ../include/class.h
userload.o: ../include/dbuf.h ../include/userload.h ../include/h.h
userload.o: ../include/ircd/memory.h ../include/ircd/send.h
crule.o: ../include/struct.h ../include/config.h ../include/setup.h
crule.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
crule.o: ../include/sys.h ../include/snprintf.h ../include/class.h
crule.o: ../include/dbuf.h ../include/h.h ../include/ircd/memory.h
crule.o: ../include/ircd/match.h ../include/ircd/string.h
md5.o: ../include/struct.h ../include/config.h ../include/setup.h
md5.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
md5.o: ../include/sys.h ../include/snprintf.h ../include/class.h
md5.o: ../include/dbuf.h ../include/ircd/md5.h
help.o: ../include/struct.h ../include/config.h ../include/setup.h
help.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
help.o: ../include/sys.h ../include/snprintf.h ../include/class.h
help.o: ../include/dbuf.h ../include/h.h ../include/ircd/memory.h
help.o: ../include/msg.h ../include/ircd/send.h ../include/ircd/string.h
version.o: ../include/struct.h ../include/config.h ../include/setup.h
version.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
version.o: ../include/sys.h ../include/snprintf.h ../include/class.h
version.o: ../include/dbuf.h ../include/patchlevel.h
s_sock.o: ../include/struct.h ../include/config.h ../include/setup.h
s_sock.o: ../include/options.h ../include/common.h ../include/ircd/cdefs.h
s_sock.o: ../include/sys.h ../include/snprintf.h ../include/class.h
s_sock.o: ../include/dbuf.h ../include/res.h ../include/numeric.h
s_sock.o: ../include/patchlevel.h ../include/sock.h ../include/h.h
s_sock.o: ../include/ircd/memory.h
memory.o: ../include/config.h ../include/setup.h ../include/options.h
memory.o: ../include/struct.h ../include/common.h ../include/ircd/cdefs.h
memory.o: ../include/sys.h ../include/snprintf.h ../include/class.h
memory.o: ../include/dbuf.h ../include/ircd/memory.h
string.o: ../include/ircd/cdefs.h ../include/ircd/string.h ../include/sys.h
string.o: ../include/setup.h
