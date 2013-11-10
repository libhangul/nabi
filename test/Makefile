# Makefile

CFLAGS = -I/usr/X11R6/include -Wall -g
CXXFLAGS = -I/usr/X11R6/include -Wall -g
LIBS = -L/usr/X11R6/lib -lX11

QT3_CXXFLAGS = -I$(QTDIR)/include
QT3_LIBS = -L$(QTDIR)/lib -lqt-mt

QT4_CXXFLAGS = $(shell pkg-config --cflags QtGui)
QT4_LIBS = $(shell pkg-config --libs QtGui)

GTK1_CFLAGS = $(shell gtk--config --cflags)
GTK1_LIBS = $(shell gtk--config --libs)

GTK2_CFLAGS = $(shell pkg-config --cflags gtk+-2.0)
GTK2_LIBS = $(shell pkg-config --libs gtk+-2.0)

GTK3_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK3_LIBS = $(shell pkg-config --libs gtk+-3.0)

all: xlib gtk2 gtk3 qt4 xim_filter.so

clean:
	rm -f xlib gtk1 gtk2 gtk3 qt3 qt4

xlib: xlib.cpp
	g++  $(CXXFLAGS) xlib.cpp -o xlib $(LIBS)

gtk1: gtk1.c
	gcc -Wall -g `gtk-config --cflags --libs` gtk1.c -o gtk1

gtk2: gtk.c
	gcc -Wall -g $(GTK2_CFLAGS) $< -o $@ $(GTK2_LIBS)

gtk3: gtk.c
	gcc -Wall -g $(GTK3_CFLAGS) $< -o $@ $(GTK3_LIBS)

qt3: qt.cpp
	g++ -Wall -g $(QT3_CXXFLAGS) $< -o $@ $(QT3_LIBS)

xim_filter.so: xim_filter.c
	gcc -Wall -g -shared -fPIC xim_filter.c -o xim_filter.so -ldl

qt4: qt4.cpp
	g++ -Wall -g -O0 $(QT4_CXXFLAGS) $< -o $@ $(QT4_LIBS)
