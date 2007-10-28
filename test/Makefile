# Makefile

CFLAGS = -I/usr/X11R6/include -Wall -g
CXXFLAGS = -I/usr/X11R6/include -Wall -g
LIBS = -L/usr/X11R6/lib -lX11

all: xlib gtk gtk1 qt xim_filter.so

clean:
	rm -f xlib gtk1 gtk qt

xlib: xlib.cpp
	g++ $(CXXFLAGS) xlib.cpp -o xlib $(LIBS)

gtk1: gtk1.c
	gcc -Wall `gtk-config --cflags --libs` gtk1.c -o gtk1

gtk: gtk.c
	gcc -Wall `pkg-config --cflags --libs gtk+-2.0` gtk.c -o gtk

qt: qt.cpp
	g++ -Wall -I$(QTDIR)/include -L$(QTDIR)/lib -lqt-mt qt.cpp -o qt

xim_filter.so: xim_filter.c
	gcc -Wall -g -shared -fPIC xim_filter.c -o xim_filter.so -ldl
