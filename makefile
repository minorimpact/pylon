CC=gcc
CFLAGS=-lev
SRCDIR=src
BINDIR=bin
DOCDIR=doc
INSTALLDIR=/usr/local/bin
MANDIR=/usr/local/man
FILES=$(SRCDIR)/main.c $(SRCDIR)/pylon.c $(SRCDIR)/servergraph.c $(SRCDIR)/valuelist.c $(SRCDIR)/daemon.c
OUTPUT=pylon

all: build 

build:
ifeq ($(shell ls /usr/include/libev/ev.h 2>/dev/null),)
	$(CC) $(CFLAGS) $(FILES) -o $(BINDIR)/$(OUTPUT)
else
	$(CC) $(CFLAGS) -D_EVSUB $(FILES) -o $(BINDIR)/$(OUTPUT)
endif

install:
	cp $(BINDIR)/$(OUTPUT) $(INSTALLDIR)
	cp $(BINDIR)/pylonstatus.pl $(INSTALLDIR)
	cp init.sample /etc/rc.d/init.d/pylon
	mkdir -p $(MANDIR)/man8
	cp $(DOCDIR)/pylon.8 $(MANDIR)/man8/
	cp $(DOCDIR)/pylonstatus.pl.8 $(MANDIR)/man8/
	gzip -f $(MANDIR)/man8/pylon.8
	gzip -f $(MANDIR)/man8/pylonstatus.pl.8

install_rpm:
	mkdir -p $(RPM_BUILD_ROOT)/$(INSTALLDIR)
	cp $(BINDIR)/$(OUTPUT) $(RPM_BUILD_ROOT)/$(INSTALLDIR)
	cp $(BINDIR)/pylonstatus.pl $(RPM_BUILD_ROOT)/$(INSTALLDIR)/
	mkdir -p $(RPM_BUILD_ROOT)/etc/rc.d/init.d
	cp init.sample $(RPM_BUILD_ROOT)/etc/rc.d/init.d/pylon
	mkdir -p $(RPM_BUILD_ROOT)/$(MANDIR)/man8
	cp $(DOCDIR)/pylon.8 $(RPM_BUILD_ROOT)/$(MANDIR)/man8/
	cp $(DOCDIR)/pylonstatus.pl.8 $(RPM_BUILD_ROOT)/$(MANDIR)/man8/
	gzip -f $(RPM_BUILD_ROOT)/$(MANDIR)/man8/pylon.8
	gzip -f $(RPM_BUILD_ROOT)/$(MANDIR)/man8/pylonstatus.pl.8
