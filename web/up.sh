#!/bin/sh

for i in $@; do
    dir=`dirname $i`
    scp $i $USER@nabi.kldp.net:/var/lib/gforge/chroot/home/groups/nabi/htdocs/$dir
done
