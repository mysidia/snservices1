#!/bin/sh
cd src
/bin/echo -n "compiling fcrypt..."
gcc -c fcrypt.c >/dev/null 2>Makefile.err
if [ $? -eq 0 ]; then
    /bin/echo " done."
    rm -f Makefile.err
    exit 0
else
    /bin/echo " compile generated errors..."
    more Makefile.err
    rm -f Makefile.err
    echo "[Press enter to continue]"
    read 'foo'
    exit -1
fi
