# Makefile

CFLAGS = -Wall -g -O0
CXXFLAGS = -Wall -g -O0

X11_CXXFLAGS = $(shell pkg-config --cflags x11)
X11_LIBS = $(shell pkg-config --libs x11)

QT3_CXXFLAGS = -I$(QTDIR)/include
QT3_LIBS = -L$(QTDIR)/lib -lqt-mt

QT4_CXXFLAGS = $(shell pkg-config --cflags QtGui)
QT4_LIBS = $(shell pkg-config --libs QtGui)

QT5_CXXFLAGS = $(shell pkg-config --cflags Qt5Widgets)
QT5_LIBS = $(shell pkg-config --libs Qt5Widgets)

GTK1_CFLAGS = $(shell gtk--config --cflags)
GTK1_LIBS = $(shell gtk--config --libs)

GTK2_CFLAGS = $(shell pkg-config --cflags gtk+-2.0)
GTK2_LIBS = $(shell pkg-config --libs gtk+-2.0)

GTK3_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK3_LIBS = $(shell pkg-config --libs gtk+-3.0)

all: xlib gtk3 qt5

clean:
	rm -f xlib xim_filter.so gtk1 gtk2 gtk3 qt5

xlib: xlib.cpp
	g++  $(CXXFLAGS) $(X11_CXXFLAGS) $< -o $@ $(X11_LIBS)

xim_filter.so: xim_filter.c
	gcc $(CFLAGS) -shared -fPIC xim_filter.c -o xim_filter.so -ldl

gtk1: gtk1.c
	gcc $(CFLAGS) $(GTK1_CFLAGS) $< -o $@ $(GTK1_LIBS)

gtk2: gtk.c
	gcc $(CFLAGS) $(GTK2_CFLAGS) $< -o $@ $(GTK2_LIBS)

gtk3: gtk.c
	gcc $(CFLAGS) $(GTK3_CFLAGS) $< -o $@ $(GTK3_LIBS)

qt3: qt.cpp
	g++ $(CXXFLAGS) $(QT3_CXXFLAGS) $< -o $@ $(QT3_LIBS)

qt4: qt4.cpp
	g++ $(CXXFLAGS) $(QT4_CXXFLAGS) $< -o $@ $(QT4_LIBS)

qt5: qt5.cpp
	g++ $(CXXFLAGS) -fPIC $(QT5_CXXFLAGS) $< -o $@ $(QT5_LIBS)
