#!/bin/sh

touch ChangeLog
autopoint
aclocal
autoheader
automake --add-missing --copy
autoconf

echo "Now you should run ./configure script with some argument"
