#!/bin/sh
gcc -c stats.c -ggdb
gcc -o stats stats.o -lxml2

./stats
perl none.pl

gnuplot nicks.plot
gnuplot emails.plot
rm -f nexp[1-2][0-9].dat nexp[0-9].dat

gnuplot cregexp.plot
gnuplot  regexp.plot

