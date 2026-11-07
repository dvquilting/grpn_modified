# Makefile by Paul Wilkins
# this will compile the program 'grpn'
# Please read the next few lines and enter the correct values.

# What compiler should be used
CC = gcc 
PKG_CONFIG ?= pkg-config
PACKAGES = gtk4 mpfr

CFLAGS = -g -O2 -Wall -Wextra `$(PKG_CONFIG) --cflags $(PACKAGES)`

DFLAGS =

# end of user configurable section



OBJS = real.o complex.o matrix.o number.o \
        funcs.o constant.o button_def.o run_cmd.o \
	main.o stack.o lcd.o setup_menu.o callback_menu.o \
	process_input.o options.o \
	setup_buttons.o callback_buttons.o editor.o \
	error.o help.o mode.o undo.o variables.o

LIBS = `$(PKG_CONFIG) --libs $(PACKAGES)` -lm
STATIC_LIBS = `$(PKG_CONFIG) --static --libs $(PACKAGES)` -lm
STATIC_LDFLAGS ?= -static

grpn:	$(OBJS)
	$(CC) $(DFLAGS) -o $@ $(OBJS) $(LIBS) 

.PHONY: static static-check
static: grpn-static

grpn-static: static-check $(OBJS)
	$(CC) $(DFLAGS) $(STATIC_LDFLAGS) -o $@ $(OBJS) $(STATIC_LIBS)

static-check:
	@missing=0; \
	for spec in 'gtk4:libgtk-4.a' 'mpfr:libmpfr.a' 'gmp:libgmp.a'; do \
		package=$${spec%%:*}; archive=$${spec#*:}; \
		libdir=`$(PKG_CONFIG) --variable=libdir $$package 2>/dev/null`; \
		if test ! -f "$$libdir/$$archive"; then \
			echo "Missing static archive: $$archive (pkg-config package $$package)"; \
			missing=1; \
		fi; \
	done; \
	if test $$missing -ne 0; then \
		echo "Install static builds of GTK 4, MPFR, GMP, and their transitive dependencies."; \
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

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $*.c

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) *.c >> .depend


clean:
	rm -f core grpn grpn-static *.o

include .depend
