#!/bin/sh
gcc -c hparse.c -ggdb
gcc -o hparse hparse.o -lxml2

gcc -c hparse.c -DHTML -ggdb
gcc -o hparse2 hparse.o -lxml2

