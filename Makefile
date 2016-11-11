# I2C tools for Linux
#
# Copyright (C) 2007-2012  Jean Delvare <jdelvare@suse.de>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

DESTDIR	?=
prefix	= /usr/local
bindir	= $(prefix)/bin
sbindir	= $(prefix)/sbin
mandir	= $(prefix)/share/man
man8dir	= $(mandir)/man8
incdir	= $(prefix)/include
libdir	= $(prefix)/lib

INSTALL		:= install
INSTALL_DATA	:= $(INSTALL) -m 644
INSTALL_DIR	:= $(INSTALL) -m 755 -d
INSTALL_PROGRAM	:= $(INSTALL) -m 755
LN		:= ln -sf
RM		:= rm -f


# CROSS_COMPILE specify the prefix used for all executables used
# during compilation. Only gcc and related bin-utils executables
# are prefixed with $(CROSS_COMPILE).
# CROSS_COMPILE can be set on the command line
# make CROSS_COMPILE=arm64-linux-
# Alternatively CROSS_COMPILE can be set in the environment.
CROSS_COMPILE	?= $(CONFIG_CROSS_COMPILE:"%"=%)

# Make variables (CC, etc...)
AS		?= $(CROSS_COMPILE)as
LD		?= $(CROSS_COMPILE)ld
CC		?= $(CROSS_COMPILE)gcc
AR		?= $(CROSS_COMPILE)ar
STRIP		?= $(CROSS_COMPILE)strip

CFLAGS		?= -O2
# When debugging, use the following instead
#CFLAGS		:= -O -g
CFLAGS		+= -Wall
SOCFLAGS	:= -fpic -D_REENTRANT $(CFLAGS)

USE_STATIC_LIB ?= 0
BUILD_STATIC_LIB ?= 1
ifeq ($(USE_STATIC_LIB),1)
BUILD_STATIC_LIB := 1
endif

KERNELVERSION	:= $(shell uname -r)

.PHONY: all strip clean install uninstall

all:

EXTRA	:=
#EXTRA	+= eeprog py-smbus
SRCDIRS	:= include lib eeprom eeprog stub tools $(EXTRA)
include $(SRCDIRS:%=%/Module.mk)
