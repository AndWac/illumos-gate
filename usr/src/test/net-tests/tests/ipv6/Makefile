#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2024 Oxide Computer Company
#
#
include $(SRC)/Makefile.master
include $(SRC)/cmd/Makefile.cmd

ROOTOPTPKG = $(ROOT)/opt/net-tests
TESTDIR = $(ROOTOPTPKG)/tests/ipv6

PROG = link-local scope-test-v4 scope-test-v6 dup_bind

PYSHEBANG = $(PYTHON3)
PYVER = $(PYTHON3_VERSION)
PYSUFFIX = $(PYTHON3_SUFFIX)

CMDS = $(PROG:%=$(TESTDIR)/%)
FILEMODE=0444
$(CMDS) := FILEMODE = 0555

include $(SRC)/test/Makefile.com

install: $(CMDS)

clobber: clean
	$(RM) $(CMDS)

$(CMDS): $(TESTDIR)

$(TESTDIR):
	$(INS.dir)

$(TESTDIR)/%: %
	$(INS.file)

$(TESTDIR)/%: %.ksh
	$(INS.rename)

$(TESTDIR)/%: %.py
	$(INS.pyfile)

