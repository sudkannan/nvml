# Copyright (c) 2014-2015, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# src/Makefile.inc -- common Makefile rules for NVM library
#

LN = ln
OBJCOPY = objcopy
MKDIR = mkdir
INSTALL = install
CP = cp
CSTYLE = ../../utils/cstyle

INCLUDE = ../include

COMMON = ../common
vpath %.c $(COMMON)

INCS += -I../include -I../common/

CFLAGS += -std=gnu99 -Wall -pthread $(EXTRA_CFLAGS) -Werror 
CFLAGS += -DSRCVERSION=\"$(SRCVERSION)\"

LDFLAGS += -Wl,-z,relro $(EXTRA_LDFLAGS)

define arch32_error_msg

##################################################
###  32-bit builds of NVML are not supported!  ###
###  Please, use 64-bit platform/compiler.     ###
##################################################

endef

LP64 := $(shell $(CC) $(CFLAGS) -dM -E -x c /dev/null | grep -Ec "__SIZEOF_LONG__.+8|__SIZEOF_POINTER__.+8" )
ifneq ($(LP64), 2)
$(error $(arch32_error_msg))
endif

ifeq ($(shell uname -m), x86_64)
ifneq ($(wildcard /usr/lib64),)
LIBDIR = usr/lib64
else
LIBDIR = usr/lib
endif
else
LIBDIR = usr/lib
endif

LIBS_DESTDIR = $(DESTDIR)/$(LIBDIR)/$(LIBS_DIR)

EAPFLAGS=-D_DISABLE_LOGGING #-D_EAP_FLUSH_ONLY
CFLAGS += $(EAPFLAGS)

ifeq ($(DEBUG),1)
CFLAGS += -g #-pg #-O0 -ggdb -DDEBUG #-pg
LIBS_DIR = nvml_debug
OBJDIR = debug
else
#CFLAGS += -g #-pg #-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
CFLAGS += -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
LIBS_DIR =
OBJDIR = nondebug
endif

DIRNAME = $(shell basename $(CURDIR))

ifeq ($(OBJDIR),$(abspath $(OBJDIR)))
objdir = $(OBJDIR)/$(DIRNAME)
else
objdir = ../$(OBJDIR)/$(DIRNAME)
endif

libdir = $(objdir)/..

LDFLAGS += -L$(libdir) -lrdpmc

ifneq ($(SOURCE),)
OBJS += $(addprefix $(objdir)/, $(patsubst $(COMMON)/%, %, $(SOURCE:.c=.o)))
endif

ifneq ($(LIBRARY_NAME),)
LIB_NAME = lib$(LIBRARY_NAME)
endif

ifneq ($(LIBRARY_SO_VERSION),)

LIB_MAP = $(LIB_NAME).map
LIB_SONAME = $(LIB_NAME).so.$(LIBRARY_SO_VERSION)
LIB_SO = $(libdir)/$(LIB_NAME).so
LIB_AR = $(libdir)/$(LIB_NAME).a
LIB_AR_UNSCOPED = $(objdir)/$(LIB_NAME)_unscoped.o
LIB_AR_ALL = $(objdir)/$(LIB_NAME)_all.o

LIB_SO_SONAME = $(LIB_SO).$(LIBRARY_SO_VERSION)

ifneq ($(LIBRARY_VERSION),)
LIB_SO_REAL = $(LIB_SO_SONAME).$(LIBRARY_VERSION)
else
$(error LIBRARY_VERSION not set)
endif

TARGET_LIBS = $(LIB_SO_REAL) $(LIB_AR)
TARGET_LINKS = $(LIB_SO_SONAME) $(LIB_SO)
endif

ifneq ($(EXTRA_TARGETS),)
EXTRA_TARGETS_CLEAN = $(EXTRA_TARGETS:=-clean)
EXTRA_TARGETS_CLOBBER = $(EXTRA_TARGETS:=-clobber)
endif

all: $(objdir) $(libdir) $(EXTRA_TARGETS) $(LIB_AR) $(LIB_SO_SONAME) $(LIB_SO_REAL) $(LIB_SO)

$(objdir) $(libdir):
	$(MKDIR) -p $@

$(LIB_SO_REAL): $(OBJS) $(EXTRA_OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,--version-script=$(LIB_MAP),-soname,$(LIB_SONAME) -o $@ $(OBJS) $(EXTRA_OBJS) $(LIBS)

$(LIB_SO_SONAME): $(LIB_SO_REAL)
	$(LN) -sf $(shell basename $<) $@

$(LIB_SO): $(LIB_SO_SONAME)
	$(LN) -sf $(shell basename $<) $@

$(LIB_AR): $(LIB_AR_ALL)
	$(AR) rv $@ $(LIB_AR_ALL)

$(LIB_AR_ALL): $(LIB_AR_UNSCOPED) $(LIB_MAP)
	$(OBJCOPY) --localize-hidden `sed -n 's/^	*\([a-zA-Z0-9_]*\);$$/-G \1/p' $(LIB_MAP)` $< $@

$(LIB_AR_UNSCOPED): $(OBJS) $(EXTRA_OBJS)
	$(LD) -o $@ -r $(OBJS) $(EXTRA_OBJS)

install: all
	$(INSTALL) -d $(LIBS_DESTDIR)
	$(INSTALL) -p -m 0755 $(TARGET_LIBS) $(LIBS_DESTDIR)
	$(CP) -d $(TARGET_LINKS) $(LIBS_DESTDIR)

uninstall:
	$(foreach f, $(TARGET_LIBS), $(RM) $(LIBS_DESTDIR)/$(notdir $(f)))
	$(foreach f, $(TARGET_LINKS), $(RM) $(LIBS_DESTDIR)/$(notdir $(f)))

clean: $(EXTRA_TARGETS_CLEAN)
	$(RM) $(OBJS)
	$(RM) $(LIB_AR_ALL) $(LIB_AR_UNSCOPED)

clobber: clean $(EXTRA_TARGETS_CLOBBER)
	$(RM) $(LIB_AR) $(LIB_SO_SONAME) $(LIB_SO_REAL) $(LIB_SO)
	$(RM) -d $(objdir)/.deps/*.P $(objdir)/.deps

cstyle:
	$(CSTYLE) -pP *.[ch]

$(objdir)/%.o: %.c
	@mkdir -p $(objdir)/.deps
	$(CC) -MD -c -o $@ $(CFLAGS) $(INCS) -fPIC $<
	@cp $(objdir)/$*.d $(objdir)/.deps/$*.P; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	    -e '/^$$/ d' -e 's/$$/ :/' < $(objdir)/$*.d >> $(objdir)/.deps/$*.P; \
	rm -f $(objdir)/$*.d

.PHONY: all clean clobber install uninstall cstyle

-include $(objdir)/.deps/*.P
