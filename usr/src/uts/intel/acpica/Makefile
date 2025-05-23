#
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#
#
#	This makefile drives the production of the ACPI CA services
#	kernel module.
#
#	intel architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE		= ../..

#
#	Define the module and object file sets.
#
MODULE		= acpica
OBJECTS		= $(ACPICA_OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)
INC_PATH        += -I$(UTSBASE)/intel/sys/acpi
INC_PATH	+= -I$(UTSBASE)/i86pc
INC_PATH	+= -I$(SRC)/common

#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(CONFMOD)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Overrides.
#
DEBUG_DEFS	+= $(DEBUG_FLGS)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CFLAGS += $(CCVERBOSE)
CERRWARN += -_gcc=-Wno-unused-variable
CERRWARN += -_gcc=-Wno-parentheses
CERRWARN += $(CNOWARN_UNINIT)
CERRWARN += -_gcc=-Wno-unused-function
SMOFF += all_func_returns

CFLAGS += -DPWRDMN -DACPI_USE_LOCAL_CACHE -DACPI_DEBUG_OUTPUT

# We are using stack pointer value there
$(OBJS_DIR)/utdebug.o := CERRWARN += -_gcc=-Wno-dangling-pointer

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/intel/Makefile.targ
