# Standard Unix

DESTDIR ?= /usr/local

TARGET = vlink
DIR = objects

CC = gcc
CCOUT = -o $(DUMMYVARIABLE)	# produces the string "-o "
COPTS = -O2 -fomit-frame-pointer -c
CONFIG =

LD = gcc
LDOUT = -o $(DUMMYVARIABLE)	# produces the string "-o "
LDOPTS =
LIBS =
INSTALL = install

include make.rules

install: vlink
	$(INSTALL) -D vlink $(prefix)/bin/vlink

install-doc: vlink.pdf
	$(INSTALL) -D vlink.pdf $(prefix)/share/doc/vlink/vlink.pdf
