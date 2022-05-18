CC        ?= gcc
#CFLAGS    ?= -std=c99 -Wall -Wextra -pedantic -Os
CFLAGS    ?= -std=c99 -w  -pedantic -Os
FEATURES  ?= -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=600 -D_XOPEN_SOURCE_EXTENDED
HEADERS   ?=
LIBPATH   ?=
DESTDIR   ?= /usr/local
MANDIR    ?= $(DESTDIR)/share/man/man1
CURSESLIB ?= ncursesw
LIBS      ?= -l$(CURSESLIB) -lpanel -lutil

all: mtm

mtm: vtparser.c mtm.c pair.c config.h rawline.c lineedit.c readlink.c log.c
	$(CC) $(CFLAGS) $(FEATURES) -o $@ $(HEADERS) vtparser.c mtm.c pair.c rawline.c lineedit.c readlink.c toml.c log.c $(LIBPATH) $(LIBS)
	strip mtm

config.h: config.def.h
	cp -i config.def.h config.h

install: mtm
	cp mtm $(DESTDIR)/bin
	cp mtm.1 $(MANDIR)

install-terminfo: mtm.ti
	tic -s -x mtm.ti

clean:
	rm -f *.o mtm
