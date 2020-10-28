# Standard Unix with gcc

DESTDIR ?= /usr/local

ifdef COMSPEC
X = .exe
endif

TARGET = vlink$X
DIR = objects
MD = mkdir

CC = gcc
CCOUT = -o $(DUMMYVARIABLE)	# produces the string "-o "
COPTS = -std=c99 -Wpedantic -O2 -fomit-frame-pointer -c
CONFIG =

LD = $(CC)
LDOUT = -o $(DUMMYVARIABLE)	# produces the string "-o "
LDOPTS =
LIBS =
INSTALL = install

include make.rules

install: $(TARGET)
	$(INSTALL) -D $(TARGET) $(DESTDIR)/bin/$(TARGET)

install-doc: vlink.pdf
	$(INSTALL) -D vlink.pdf $(DESTDIR)/share/doc/vlink/vlink.pdf
