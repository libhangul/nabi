# Makefile

CFLAGS = -I/usr/X11R6/include -Wall -g
CXXFLAGS = -I/usr/X11R6/include -Wall -g
LIBS = -L/usr/X11R6/lib -lX11

QT4_CXXFLAGS = $(shell pkg-config --cflags QtGui)
QT4_LIBS = $(shell pkg-config --libs QtGui)

all: xlib gtk gtk1 qt xim_filter.so

clean:
	rm -f xlib gtk1 gtk qt

xlib: xlib.cpp
	g++  $(CXXFLAGS) xlib.cpp -o xlib $(LIBS)

gtk1: gtk1.c
	gcc -Wall -g `gtk-config --cflags --libs` gtk1.c -o gtk1

gtk: gtk.c
	gcc -Wall -g `pkg-config --cflags --libs gtk+-2.0` gtk.c -o gtk

qt: qt.cpp
	g++ -Wall -I$(QTDIR)/include -L$(QTDIR)/lib -lqt-mt qt.cpp -o qt

xim_filter.so: xim_filter.c
	gcc -Wall -g -shared -fPIC xim_filter.c -o xim_filter.so -ldl

qt4: qt4.cpp
	g++ -Wall -g -O0 $(QT4_CXXFLAGS) $< -o $@ $(QT4_LIBS)
