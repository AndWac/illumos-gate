#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2015 RackTop Systems.
#
# Copyright 2020 Joyent, Inc.
#

MYPROG = svc.configd
MYOBJS = \
	backend.o \
	configd.o \
	client.o \
	file_object.o \
	maindoor.o \
	object.o \
	rc_node.o \
	snapshot.o

PROG = $(MYPROG)
OBJS = $(MYOBJS)

SRCS = $(MYOBJS:%.o=%.c)

include ../../Makefile.cmd
include ../../Makefile.ctf

NATIVE_BUILD=$(POUND_SIGN)
$(NATIVE_BUILD)PROG = $(MYPROG:%=%-native)
$(NATIVE_BUILD)OBJS = $(MYOBJS:%.o=%-native.o)

MYCPPFLAGS = -I. -I../common -I../../../common/svc \
	-I$(ROOT)/usr/include/sqlite-sys -D_REENTRANT
CPPFLAGS += $(MYCPPFLAGS)
CFLAGS	+= $(CCVERBOSE)
CERRWARN += -_gcc=-Wno-parentheses
CERRWARN += -_gcc=-Wno-type-limits
CERRWARN += -_gcc=-Wno-unused-label
CERRWARN += -_gcc=-Wno-unused-variable
CERRWARN += -_gcc=-Wno-unused-function
CERRWARN += $(CNOWARN_UNINIT)

# strange false positive
SMOFF += free

MYLDLIBS = -lumem -luutil
LDLIBS	+= -lsecdb -lbsm $(MYLDLIBS)

CLOBBERFILES +=	$(MYPROG:%=%-native)

LIBUUTIL	= $(SRC)/lib/libuutil
LIBSCF		= $(SRC)/lib/libscf

SCRIPTFILE	= restore_repository
ROOTSCRIPTFILE	= $(ROOTLIBSVCBIN)/$(SCRIPTFILE)

#
# Native variant (used in ../seed).
#
$(NATIVE_BUILD)NATIVE_LIBS += libumem.so libc.so
$(NATIVE_BUILD)CC =	$(NATIVECC)
$(NATIVE_BUILD)LD =	$(NATIVELD)
$(NATIVE_BUILD)CFLAGS =	$(NATIVE_CFLAGS)
$(NATIVE_BUILD)CPPFLAGS = $(MYCPPFLAGS) -I$(LIBUUTIL)/common -I$(LIBSCF)/inc
$(NATIVE_BUILD)CPPFLAGS += -DNATIVE_BUILD
$(NATIVE_BUILD)LDFLAGS = $(BDIRECT)
$(NATIVE_BUILD)LDLIBS = -L$(ADJUNCT_PROTO)/usr/lib -R$(ADJUNCT_PROTO)/usr/lib \
			-L$(LIBUUTIL)/native -R $(LIBUUTIL)/native $(MYLDLIBS)

DIRMODE = 0755
FILEMODE = 0555

OBJSQLITE =
LIBSQLITE = -lsqlite-sys
$(NATIVE_BUILD)OBJSQLITE = $(ROOT)/lib/libsqlite-native.o
$(NATIVE_BUILD)LIBSQLITE =

OBJS += $(OBJSQLITE)
LDLIBS += $(LIBSQLITE)

install := TARGET = install
clobber := TARGET = clobber

.KEEP_STATE:
.PARALLEL: $(MYOBJS) $(MYOBJS:%.o=%-native.o)

all: $(PROG)

native: FRC
	@cd $(LIBUUTIL)/native; pwd; $(MAKE) install
	@NATIVE_BUILD= $(MAKE) all

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

%-native.o: %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(ROOTLIBSVCBIN)/%: %.sh
	$(INS.rename)

install: all $(ROOTLIBSVCBINPROG) $(ROOTVARSADMFILE) $(ROOTSCRIPTFILE)

clean: FRC
	$(RM) $(MYOBJS) $(MYOBJS:%.o=%-native.o)

clobber:

include ../../Makefile.targ

FRC:
