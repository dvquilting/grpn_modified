# Makefile by Paul Wilkins
# this will compile the program 'grpn'
# Please read the next few lines and enter the correct values.

# What compilers should be used
CC = gcc
CXX = g++
PKG_CONFIG ?= pkg-config
# Qt major version to build against: 5 or 6 (e.g. "make QT=6")
QT ?= 5

# the calculator core is C and needs only MPFR; the Qt GUI is C++
CORE_PACKAGES = mpfr
GUI_PACKAGES = Qt$(QT)Widgets
PACKAGES = $(GUI_PACKAGES) $(CORE_PACKAGES)

CFLAGS = -g -O2 -Wall -Wextra `$(PKG_CONFIG) --cflags $(CORE_PACKAGES)`
# Qt 6 headers need C++17; Qt 5 needs C++11
CXXSTD_5 = -std=c++11
CXXSTD_6 = -std=c++17
# Qt is normally built with -reduce-relocations, which needs -fPIC
# (-isystem keeps warnings in, and dependencies on, the Qt headers out)
CXXFLAGS = -g -O2 -Wall -Wextra $(CXXSTD_$(QT)) -fPIC `$(PKG_CONFIG) --cflags $(PACKAGES) | sed 's/-I/-isystem /g'`

DFLAGS =

# end of user configurable section



ifeq ($(filter 5 6,$(QT)),)
$(error QT must be 5 or 6, not "$(QT)")
endif

GUI_OBJS = main.o lcd_qt.o setup_menu.o options.o setup_buttons.o help.o

OBJS = real.o complex.o matrix.o number.o \
        funcs.o constant.o button_def.o run_cmd.o \
	main.o stack.o lcd.o lcd_qt.o setup_menu.o callback_menu.o \
	process_input.o options.o \
	setup_buttons.o callback_buttons.o editor.o \
	error.o help.o mode.o undo.o variables.o settings.o

LIBS = `$(PKG_CONFIG) --libs $(PACKAGES)` -lm
STATIC_LIBS = `$(PKG_CONFIG) --static --libs $(PACKAGES)` -lm
STATIC_LDFLAGS ?= -static

grpn:	$(OBJS)
	$(CXX) $(DFLAGS) -o $@ $(OBJS) $(LIBS)

.PHONY: static static-check
static: grpn-static

grpn-static: static-check $(OBJS)
	$(CXX) $(DFLAGS) $(STATIC_LDFLAGS) -o $@ $(OBJS) $(STATIC_LIBS)

static-check:
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

# The GUI objects are rebuilt when QT changes: each build leaves a stamp
# file naming the Qt version its objects were compiled for.
QT_STAMP = .qt$(QT)-stamp
$(GUI_OBJS): $(QT_STAMP)
$(QT_STAMP):
	rm -f .qt5-stamp .qt6-stamp
	touch $@

.SUFFIXES: .cpp
.c.o:
	$(CC) -c -o $@ $(CFLAGS) $*.c
.cpp.o:
	$(CXX) -c -o $@ $(CXXFLAGS) $*.cpp

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) $(filter-out test.c,$(wildcard *.c)) >> .depend
	$(CXX) -MM $(CXXFLAGS) $(wildcard *.cpp) >> .depend


clean:
	rm -f core grpn grpn-static *.o .qt5-stamp .qt6-stamp

include .depend
