#!/bin/sh

aclocal -I m4
autoheader
automake --add-missing --copy
autoconf

echo "Now you should run ./configure script with some argument"
