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

CC = @CXX@
CXX = @CXX@
CPPUNIT_PREFIX = @CPPUNIT_PREFIX@

SVOBJS = ../akill.o ../db.o ../help.o ../parse.o ../stuff.o \
         ../chanserv.o ../email.o ../infoserv.o ../memoserv.o \
         ../passwd.o ../timer.o ../clone.o ../gameserv.o ../interp.o \
         ../nickserv.o ../server.o ../credits.o ../hash.o ../log.o \
         ../operserv.o ../string.o ../conf.o ../signal.o ../hash/libmd5.a \
	 ../sipc.o ../mass.o ../timestr.o ../chantrig.o

DEFAULT_INCLUDES = -I./
INCLUDES = -I./ -I../ -I- -I/usr/include -I@includedir@ -I${CPPUNIT_PREFIX}/include

EXTRA_PROGRAMS = tests tool_cmd
tests_SOURCES = email_test.cc stuff_test.cc chanserv_test.cc run_test.cc \
timestr_test.cc passwd_test.cc

tests_LDFLAGS = -L${CPPUNIT_PREFIX}/lib
tests_LDADD =  -lcppunit $(SVOBJS)

tool_cmd_SOURCES = tool_cmd.c
tool_cmd_LDADD = $(SVOBJS)

OBJS = email_test.o stuff_test.o chanserv_test.o run_test.o
#CFLAGS = @CFLAGS@ -I../
#CXXFLAGS = @CFLAGS@ -I../

#tests: $(SVOBJS) $(OBJS)
#	$(CXX) @DEFS@ @INCLUDES@ -o tests -lcppunit $(OBJS) $(SVOBJS)

$(SVOBJS) :
	(cd ../;make)

check:  tests
	LD_LIBRARY_PATH=${CPPUNIT_PREFIX}/lib ./tests

#tests: $(OBJS)
#	$(CXX) -o tests -lcppunit $(OBJS) $(SVOBJS)

clean:
	rm -f $(OBJS)
