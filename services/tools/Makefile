CC      = gcc
DEFS	= $(DEFINES)
CSDB	= ../chanserv/chanserv.db
NSDB	= ../nickserv/nickserv.db

#DEFINES = -DCSDATAFILE=\""$(CSDB)"\" \
#	  -DNSDATAFILE=\""$(NSDB)"\"

CFLAGS  = -I.. $(DEFS) -Wall -g 
RM	= /bin/rm

all: chanparse nickparse servop mul

.c.o:
	$(CC) $(CFLAGS) -c $<

chanparse: chanparse.c
	$(CC) $(CFLAGS) -o chanparse chanparse.c

nickparse: nickparse.c
	$(CC) $(CFLAGS) -o nickparse nickparse.c

servop: servop.c
	$(CC) $(CFLAGS) -o servop servop.c

mul: mul.c
	$(CC) $(CFLAGS) -o mul mul.c

clean::
	$(RM) -f chanparse nickparse *.o *.core core
