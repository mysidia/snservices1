# $Id$
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

DEFS = @DEFS@ $(DEFINES)
CFLAGS  = @CFLAGS@ $(DEFS) $(WFLAGS) -g
LDFLAGS = @LDFLAGS@
INCLUDES = -I. -I- -I/usr/include -I@includedir@
DEFAULT_INCLUDES = 
#DEFAULT_INCLUDES =  -I. -I$(srcdir) -I$(top_builddir)

# Warning flags
WFLAGS    = -Wall -Wpointer-arith \
-Wcast-qual \
-Wcast-align \
-Wmissing-declarations \
-Winline

noinst_LIBRARIES = libmd5.a
libmd5_a_SOURCES = md5.c passwd.c

bin_PROGRAMS = md5-prep
md5_prep_SOURCES = md5-prep.c
md5_prep_LDADD = libmd5.a
