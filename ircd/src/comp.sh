#!/bin/sh
# execute a compilation command...
FNAME="$1"
shift 1

IND="`echo $FNAME | awk '{print index($1, ".o")}'`"

if [ $IND -eq 0 ] ; then
FSNAME="`echo $FNAME | awk '{printf("%s\n", $1)}'`"
FPRINT="`echo $FNAME | awk '{printf("%5s\n", $1)}'`"
FCPRINT="`echo $FNAME | awk '{printf("%16s\n", $1)}'`..."
else
FSNAME="`echo $FNAME | awk -F'.' '{printf("%s.c\n", $1)}'`"
FPRINT="`echo $FNAME | awk -F'.' '{printf("%5s.c\n", $1)}'`"
FCPRINT="`echo $FNAME | awk -F'.' '{printf("%12s.c\n", $1)}'`..."
fi

if [ "$FNAME" != "ircd" ] ; then
/bin/echo -n "compiling $FCPRINT"
else
/bin/echo -n "linking $FCPRINT"
fi


$* >/dev/null 2>Makefile.err0
ERCODE="$?"
if [ "$ERCODE" -eq 0 ]; then
      if [ -f clock ] ; then
        echo "$FSNAME">>clock
      fi

        rm -f Makefile.err0
        echo "done."
else
        echo "failed"
        echo "gcc returned error code $ERCODE"
        echo "cmd: '$*'"
        echo "====================================================="
        cat Makefile.err0 >> Makefile.err
        cat Makefile.err
        rm -f Makefile.err0
        exit $ERCODE
fi
