# Makefile by Paul Wilkins
# this will compile the program 'grpn'
# Please read the next few lines and enter the correct values.

# What compilers should be used
CC = gcc
CXX = g++
PKG_CONFIG ?= pkg-config
# Qt major version to build against: 6 or 5 (e.g. "make QT=5")
QT ?= 6

# To link Qt statically, set QT_STATIC_PREFIX to the install prefix of
# a static Qt build (e.g. "make QT_STATIC_PREFIX=/opt/qt6-static").
# Qt's platform plugins are then linked in too: QT_STATIC_PLUGINS lists
# them, from xcb (X11), wayland, offscreen and minimal.  Other libraries
# (libc, X11, fontconfig, MPFR, ...) stay shared.
QT_STATIC_PREFIX ?=
QT_STATIC_PLUGINS ?= xcb

# the calculator core is C and needs only MPFR; the Qt GUI is C++
CORE_PACKAGES = mpfr
GUI_PACKAGES = Qt$(QT)Widgets

CFLAGS = -g -O2 -Wall -Wextra `$(PKG_CONFIG) --cflags $(CORE_PACKAGES)`
# Qt 6 headers need C++17; Qt 5 needs C++11
CXXSTD_5 = -std=c++11
CXXSTD_6 = -std=c++17
# Qt is normally built with -reduce-relocations, which needs -fPIC
# (-isystem keeps warnings in, and dependencies on, the Qt headers out)
CXXFLAGS = -g -O2 -Wall -Wextra $(CXXSTD_$(QT)) -fPIC $(QT_CFLAGS) `$(PKG_CONFIG) --cflags $(CORE_PACKAGES)`

DFLAGS =

# end of user configurable section



ifeq ($(filter 5 6,$(QT)),)
$(error QT must be 5 or 6, not "$(QT)")
endif

GUI_OBJS = main.o lcd_qt.o setup_menu.o options.o setup_buttons.o help.o

ifeq ($(QT_STATIC_PREFIX),)
QT_CFLAGS = `$(PKG_CONFIG) --cflags $(GUI_PACKAGES) | sed 's/-I/-isystem /g'`
QT_LIBS = `$(PKG_CONFIG) --libs $(GUI_PACKAGES)`
else
ifeq ($(wildcard $(QT_STATIC_PREFIX)/lib/libQt$(QT)Widgets.prl),)
$(error $(QT_STATIC_PREFIX)/lib/libQt$(QT)Widgets.prl not found: QT_STATIC_PREFIX must be the prefix of a static Qt $(QT) build)
endif
QT_CFLAGS = $(foreach d,. QtCore QtGui QtWidgets,-isystem $(QT_STATIC_PREFIX)/include/$(d))
QT_LIBS = `sh ./qt-static-libs.sh libs $(QT_STATIC_PREFIX) $(QT) $(QT_STATIC_PLUGINS)`
# registers the linked-in plugins with Qt
GUI_OBJS += qt_static_plugins.o
endif

OBJS = real.o complex.o matrix.o number.o \
        funcs.o constant.o button_def.o run_cmd.o \
	stack.o lcd.o callback_menu.o process_input.o \
	callback_buttons.o editor.o \
	error.o mode.o undo.o variables.o settings.o \
	$(GUI_OBJS)

LIBS = $(QT_LIBS) `$(PKG_CONFIG) --libs $(CORE_PACKAGES)` -lm
STATIC_LIBS = `$(PKG_CONFIG) --static --libs $(GUI_PACKAGES) $(CORE_PACKAGES)` -lm
STATIC_LDFLAGS ?= -static

grpn:	$(OBJS)
	$(CXX) $(DFLAGS) -o $@ $(OBJS) $(LIBS)

.PHONY: static static-check
static: grpn-static

grpn-static: static-check $(OBJS)
	$(CXX) $(DFLAGS) $(STATIC_LDFLAGS) -o $@ $(OBJS) $(STATIC_LIBS)

static-check:
	@if test -n "$(QT_STATIC_PREFIX)"; then \
		echo "'make static' links everything statically from pkg-config packages."; \
		echo "To link only Qt statically, run 'make QT_STATIC_PREFIX=$(QT_STATIC_PREFIX)'."; \
		exit 1; \
	fi
	@missing=0; \
	for spec in 'Qt$(QT)Widgets:libQt$(QT)Widgets.a' 'Qt$(QT)Gui:libQt$(QT)Gui.a' 'Qt$(QT)Core:libQt$(QT)Core.a' 'mpfr:libmpfr.a' 'gmp:libgmp.a'; do \
		package=$${spec%%:*}; archive=$${spec#*:}; \
		libdir=`$(PKG_CONFIG) --variable=libdir $$package 2>/dev/null`; \
		if test ! -f "$$libdir/$$archive"; then \
			echo "Missing static archive: $$archive (pkg-config package $$package)"; \
			missing=1; \
		fi; \
	done; \
	if test $$missing -ne 0; then \
		echo "Install static builds of Qt $(QT), MPFR, GMP, and their transitive dependencies."; \
		exit 1; \
	fi

pure:	$(OBJS)
	$(PURIFY) $(CC) $(DFLAGS) -o $@ $(OBJS) $(LIBS) 

install:	grpn
	mkdirhier $(PREFIX)/bin
	cp grpn $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/grpn
	mkdirhier $(PREFIX)/man/man1
	cp grpn.1 $(PREFIX)/man/man1
	chmod 644 $(PREFIX)/man/man1/grpn.1

# The GUI objects are rebuilt when the Qt settings change: .qt-config
# records the settings they were compiled with, and is rewritten (and
# so becomes newer than them) only when they change.
QT_CONFIG = $(QT) $(QT_STATIC_PREFIX) $(if $(QT_STATIC_PREFIX),$(QT_STATIC_PLUGINS))
$(GUI_OBJS): .qt-config
.qt-config: FORCE
	@echo '$(QT_CONFIG)' | cmp -s - $@ || echo '$(QT_CONFIG)' > $@
.PHONY: FORCE
FORCE:

qt_static_plugins.cpp: .qt-config qt-static-libs.sh
	sh ./qt-static-libs.sh imports $(QT_STATIC_PLUGINS) > $@.tmp || { rm -f $@.tmp; exit 1; }
	mv $@.tmp $@

.SUFFIXES: .cpp
.c.o:
	$(CC) -c -o $@ $(CFLAGS) $*.c
.cpp.o:
	$(CXX) -c -o $@ $(CXXFLAGS) $*.cpp

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) $(filter-out test.c,$(wildcard *.c)) >> .depend
	$(CXX) -MM $(CXXFLAGS) $(filter-out qt_static_plugins.cpp,$(wildcard *.cpp)) >> .depend


clean:
	rm -f core grpn grpn-static *.o .qt-config qt_static_plugins.cpp

include .depend
