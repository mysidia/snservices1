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
# $Id$
# From Id: Makefile.in,v 1.35.2.5 2001/10/20 18:39:52 mysidia Exp

SUBDIRS=hash tests

bin_PROGRAMS = services
EXTRA_PROGRAMS = services-ef read2

CC	= @CXX@
DEFS = @DEFS@ @MD5_DEF@ $(DEFINES)

CFGPATH = @CFGPATH@
SENDMAIL = @SENDMAIL@
HELP_PATH = @HELP_PATH@

DEFINES = 


CFLAGS  = @CFLAGS@ $(DEFS) $(WFLAGS) -g
CXXFLAGS = $(CFLAGS)

DEFAULT_INCLUDES =

INCLUDES = -I./ -I- -I/usr/include -I@includedir@

LIBS = @LIBS@ ${MD5_LIB}
LDFLAGS = @LDFLAGS@

# Warning flags
WFLAGS  = -Wall -Wpointer-arith -Wcast-qual -Wcast-align \
-Woverloaded-virtual -Winline -Wno-format-y2k 

services_SOURCES = akill.c chanserv.c clone.c db.c hash.c help.c \
main.c parse.cc memoserv.c nickserv.c operserv.c server.c stuff.c \
timer.c infoserv.c credits.c gameserv.c passwd.c interp.cc log.cc \
email.cc string.cc conf.c mass.c signal.c chantrig.c sipc.cc timestr.cc \
timestr.h

services_ef_SOURCES = $(services_SOURCES)
#services_LDADD = hash/libmd5.a

services_ef_LDADD = hash/md5.a -L/usr/pkg/lib -lefence

read2_SOURCES = read2.c

#helptest_SOURCES = helpfile.cc
#$(CC)	$(CFLAGS) -DTEST_HELPFILE -o helptest helpfile.cc $(LDFLAGS)

