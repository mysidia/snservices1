#!/bin/sh
cat << EOF > ./LICENSE.dox
/** \page LICENSE
 \verbatim
EOF

cat ../../LICENSE |fgrep -v '$Id' >> ./LICENSE.dox

cat << EOF >> ./LICENSE.dox
 \endverbatim
*/
EOF

cd ../../
doxygen tools/Doxyfile
