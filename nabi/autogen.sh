#!/bin/sh

glib-gettextize
aclocal
autoheader
automake --add-missing --copy
autoconf

echo "Now you should run ./configure script with some argument"
