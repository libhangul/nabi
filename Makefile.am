SUBDIRS = IMdkit src tables themes po

nabilogodir = @NABI_DATA_DIR@
nabilogo_DATA = nabi.png nabi.svg nabi-about.png

testclients = \
	test/Makefile	\
	test/xlib.cpp	\
	test/gtk.c	\
	test/gtk1.c	\
	test/qt.cpp

EXTRA_DIST = config.rpath $(nabilogo_DATA) $(testclients)

log:
	lastdate=`head -n 2 ChangeLog | grep -oe '[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}'` ; \
	unset LC_ALL; \
	export LANG=C ; \
	export LC_CTYPE=ko_KR.UTF-8 ; \
	mv -f ChangeLog ChangeLog.old ; \
	svn log -v -r HEAD:{$$lastdate} > ChangeLog ; \
	cat ChangeLog.old >> ChangeLog
