# $Id$

#
# Convert a SorceryNet services brain dump (nick expire time, etc)
# into spiffy graphs.
#

all: NickExpire.gif NickLength.gif

NickExpire.pbm NickLength.pbm: NickExpire.dat NickLength.dat serv.gnuplot
	gnuplot serv.gnuplot

NickExpire.gif: NickExpire.pbm
	ppmtogif -interlace < $> > $@

NickLength.gif: NickLength.pbm
	ppmtogif -interlace < $> > $@

clean:
	rm -f NickExpire.pbm NickExpire.gif
	rm -f NickLength.pbm NickLength.gif
