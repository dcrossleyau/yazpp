#!/bin/sh
# $Id: buildconf.sh,v 1.10 2003-05-06 12:09:53 adam Exp $
set -x
dir=`aclocal --print-ac-dir`
if [ -f $dir/yaz.m4 ]; then
	aclocal
else
	aclocal -I . 
fi
libtoolize --force 
automake -a 
automake -a 
autoconf
if [ -f config.cache ]; then
	rm config.cache
fi
