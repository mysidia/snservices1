# Generated automatically from Makefile.in by configure.
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
MAKEDEPEND=/usr/X11R6/bin/makedepend
#CC=gcc
#normal

LINTFLAGS=-hba

OBJS=bsd.o dbuf.o packet.o send.o match.o parse.o support.o channel.o \
	class.o hash.o ircd.o list.o res.o s_auth.o s_bsd.o s_conf.o \
	s_debug.o s_err.o s_misc.o s_numeric.o s_serv.o s_user.o s_service.o \
	whowas.o userload.o crule.o help.o version.o s_sock.o \
	snprintf.o numnicks.o $(RES) $(STRTOUL)

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
	 ../include/common.h crule.c chkconf.c
	$(CC) $(DFLAGS) $(CFLAGS) -DCR_CHKCONF -o chkcrule.o -c crule.c
	$(CC) -ggdb $(CFLAGS) -DCR_CHKCONF -o chkmatch.o -c match.c
	$(CC) $(DFLAGS) $(CFLAGS) chkconf.c chkmatch.o chkcrule.o \
	$(LDFLAGS) $(IRCDLIBS) -o chkconf

saber: $(SRC)
	#load -I../include $(SRC) version.c $(IRCDLIBS)

lint:
	lint $(LINTFLAGS) -I../include $(SRC) | egrep -v 'sendto_|debug'

parse.o: parse.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS)      -o parse.o -c parse.c
bsd.o: bsd.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS)      -o bsd.o -c bsd.c
dbuf.o: dbuf.c $(INCLUDES) ../include/dbuf.h
	$(CC) $(XCFLAGS) $(CFLAGS)      -o dbuf.o -c dbuf.c
packet.o: packet.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS)      -o packet.o -c packet.c
send.o: send.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS)      -o send.o -c send.c
match.o: match.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS)      -o match.o -c match.c
support.o: support.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS)      -o support.o -c support.c
userload.o: userload.c $(INCLUDES)
	$(CC) $(XCFLAGS) $(CFLAGS)      -o userload.o -c userload.c
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

distclean: clean
	$(RM) -f Makefile options.h

distclean: clean

depend:
	@echo "Making dependencies, please wait..."
	#$(MAKEDEPEND) -I${INCLUDEDIR} -I.. -I../md5 -f Makefile.in ${SRC}
	@echo 'Making dependencies.  This takes a while'
	@sed -n '1,/^# MakeDep here:$$/p' < Makefile.in > tmp
	@mv -f tmp Makefile.in
	@cc -MM -I../include -I.. -I../md5 ${SRC} >> Makefile.in
	(cd ../;sh ./config.status)

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

# DO NOT DELETE THIS LINE -- make depend depends on it.
# MakeDep Here:
Config.o: Config.c
fcrypt.o: fcrypt.c
foo.o: foo.c
main.o: main.c
maskutil.o: maskutil.c
snprintf.o: snprintf.c
Config.o: Config.c
bsd.o: bsd.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
channel.o: channel.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/channel.h \
 ../include/hash.h ../include/h.h
chkconf.o: chkconf.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h
class.o: class.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/h.h
crule.o: crule.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
dbuf.o: dbuf.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h
fcrypt.o: fcrypt.c
foo.o: foo.c
hash.o: hash.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/hash.h ../include/h.h
help.o: help.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h \
 ../include/msg.h
ircd.o: ircd.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/userload.h ../include/h.h
list.o: list.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h \
 ../include/numeric.h
list2.o: list2.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
main.o: main.c
maskutil.o: maskutil.c
match.o: match.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h
md5.o: md5.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h
numnicks.o: numnicks.c ../include/numnicks.h ../include/struct.h \
 ../include/config.h ../setup.h ../include/options.h \
 ../include/snprintf.h ../include/common.h ../include/sys.h \
 ../include/class.h ../include/dbuf.h ../include/numeric.h \
 ../include/msg.h ../include/channel.h ../include/userload.h
packet.o: packet.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/msg.h ../include/h.h
parse.o: parse.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/msg.h ../include/numeric.h ../include/h.h
res.o: res.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/h.h \
 ../include/nameser.h ../include/resolv.h
res_comp.o: res_comp.c ../include/common.h ../include/sys.h ../setup.h \
 ../include/nameser.h
res_init.o: res_init.c ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/nameser.h ../include/resolv.h
res_mkquery.o: res_mkquery.c ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/sys.h \
 ../include/nameser.h ../include/resolv.h
res_skipname.o: res_skipname.c ../include/nameser.h
s_auth.o: s_auth.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/patchlevel.h \
 ../include/sock.h ../include/h.h
s_bsd.o: s_bsd.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/patchlevel.h \
 ../include/inet.h ../include/nameser.h ../include/resolv.h \
 ../include/sock.h ../include/h.h
s_conf.o: s_conf.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/h.h
s_debug.o: s_debug.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/whowas.h \
 ../include/hash.h ../include/h.h
s_err.o: s_err.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/s_err.h
s_misc.o: s_misc.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/userload.h ../include/h.h
s_numeric.o: s_numeric.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/h.h
s_serv.o: s_serv.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/msg.h ../include/channel.h \
 ../include/userload.h ../include/patchlevel.h ../md5/md5_iface.h \
 ../include/h.h
s_service.o: s_service.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/msg.h \
 ../include/channel.h ../include/userload.h ../include/h.h
s_sock.o: s_sock.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/patchlevel.h \
 ../include/sock.h ../include/h.h
s_user.o: s_user.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/msg.h ../include/channel.h \
 ../include/userload.h ../md5/md5_iface.h ../include/h.h
send.o: send.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
snprintf.o: snprintf.c
strtoul.o: strtoul.c ../setup.h
support.o: support.c ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/struct.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h
userload.o: userload.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/userload.h ../include/h.h
version.o: version.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/patchlevel.h
whowas.o: whowas.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/whowas.h ../include/h.h
Config.o: Config.c
bsd.o: bsd.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
channel.o: channel.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/channel.h \
 ../include/hash.h ../include/h.h
chkconf.o: chkconf.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h
class.o: class.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/h.h
crule.o: crule.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
dbuf.o: dbuf.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h
fcrypt.o: fcrypt.c
foo.o: foo.c
hash.o: hash.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/hash.h ../include/h.h
help.o: help.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h \
 ../include/msg.h
ircd.o: ircd.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/userload.h ../include/h.h
list.o: list.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h \
 ../include/numeric.h
list2.o: list2.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
main.o: main.c
maskutil.o: maskutil.c
match.o: match.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h
md5.o: md5.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h
numnicks.o: numnicks.c ../include/numnicks.h ../include/struct.h \
 ../include/config.h ../setup.h ../include/options.h \
 ../include/snprintf.h ../include/common.h ../include/sys.h \
 ../include/class.h ../include/dbuf.h ../include/numeric.h \
 ../include/msg.h ../include/channel.h ../include/userload.h
packet.o: packet.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/msg.h ../include/h.h
parse.o: parse.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/msg.h ../include/numeric.h ../include/h.h
res.o: res.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/h.h \
 ../include/nameser.h ../include/resolv.h
res_comp.o: res_comp.c ../include/common.h ../include/sys.h ../setup.h \
 ../include/nameser.h
res_init.o: res_init.c ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/nameser.h ../include/resolv.h
res_mkquery.o: res_mkquery.c ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/sys.h \
 ../include/nameser.h ../include/resolv.h
res_skipname.o: res_skipname.c ../include/nameser.h
s_auth.o: s_auth.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/patchlevel.h \
 ../include/sock.h ../include/h.h
s_bsd.o: s_bsd.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/patchlevel.h \
 ../include/inet.h ../include/nameser.h ../include/resolv.h \
 ../include/sock.h ../include/h.h
s_conf.o: s_conf.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/h.h
s_debug.o: s_debug.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/whowas.h \
 ../include/hash.h ../include/h.h
s_err.o: s_err.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/s_err.h
s_misc.o: s_misc.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/userload.h ../include/h.h
s_numeric.o: s_numeric.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/h.h
s_serv.o: s_serv.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/msg.h ../include/channel.h \
 ../include/userload.h ../include/patchlevel.h ../md5/md5_iface.h \
 ../include/h.h
s_service.o: s_service.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/msg.h \
 ../include/channel.h ../include/userload.h ../include/h.h
s_sock.o: s_sock.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/patchlevel.h \
 ../include/sock.h ../include/h.h
s_user.o: s_user.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/msg.h ../include/channel.h \
 ../include/userload.h ../md5/md5_iface.h ../include/h.h
send.o: send.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
snprintf.o: snprintf.c
strtoul.o: strtoul.c ../setup.h
support.o: support.c ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/struct.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h
userload.o: userload.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/userload.h ../include/h.h
version.o: version.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/patchlevel.h
whowas.o: whowas.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/whowas.h ../include/h.h
bsd.o: bsd.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
dbuf.o: dbuf.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h
packet.o: packet.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/msg.h ../include/h.h
send.o: send.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
match.o: match.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h
parse.o: parse.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/msg.h ../include/numeric.h ../include/h.h
support.o: support.c ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/struct.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h
channel.o: channel.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/channel.h \
 ../include/hash.h ../include/h.h
class.o: class.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/h.h
hash.o: hash.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/hash.h ../include/h.h
ircd.o: ircd.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/userload.h ../include/h.h
list.o: list.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h \
 ../include/numeric.h
res.o: res.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/h.h \
 ../include/nameser.h ../include/resolv.h
s_auth.o: s_auth.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/patchlevel.h \
 ../include/sock.h ../include/h.h
s_bsd.o: s_bsd.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/patchlevel.h \
 ../include/inet.h ../include/nameser.h ../include/resolv.h \
 ../include/sock.h ../include/h.h
s_conf.o: s_conf.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/h.h
s_debug.o: s_debug.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/whowas.h \
 ../include/hash.h ../include/h.h
s_err.o: s_err.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/s_err.h
s_misc.o: s_misc.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/userload.h ../include/h.h
s_numeric.o: s_numeric.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/h.h
s_serv.o: s_serv.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/msg.h ../include/channel.h \
 ../include/userload.h ../include/patchlevel.h ../md5/md5_iface.h \
 ../include/h.h
s_user.o: s_user.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/msg.h ../include/channel.h \
 ../include/userload.h ../md5/md5_iface.h ../include/h.h
s_service.o: s_service.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/numeric.h ../include/msg.h \
 ../include/channel.h ../include/userload.h ../include/h.h
whowas.o: whowas.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/numeric.h ../include/whowas.h ../include/h.h
userload.o: userload.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/userload.h ../include/h.h
crule.o: crule.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h
help.o: help.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h ../include/h.h \
 ../include/msg.h
version.o: version.c ../include/struct.h ../include/config.h \
 ../setup.h ../include/options.h ../include/snprintf.h \
 ../include/common.h ../include/sys.h ../include/class.h \
 ../include/dbuf.h ../include/patchlevel.h
s_sock.o: s_sock.c ../include/struct.h ../include/config.h ../setup.h \
 ../include/options.h ../include/snprintf.h ../include/common.h \
 ../include/sys.h ../include/class.h ../include/dbuf.h \
 ../include/res.h ../include/numeric.h ../include/patchlevel.h \
 ../include/sock.h ../include/h.h
snprintf.o: snprintf.c
numnicks.o: numnicks.c ../include/numnicks.h ../include/struct.h \
 ../include/config.h ../setup.h ../include/options.h \
 ../include/snprintf.h ../include/common.h ../include/sys.h \
 ../include/class.h ../include/dbuf.h ../include/numeric.h \
 ../include/msg.h ../include/channel.h ../include/userload.h
