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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2013 Nexenta Systems, Inc.  All rights reserved.
#

PERLFILES= \
	ao_gendisp

include ../../../Makefile.uts

.KEEP_STATE:

def all install setup: ao_gendisp.pl $(PERLFILES)

# We are sharing the ao_gendisp source with the i86pc
AO_GENDISP_PL=../../../i86pc/cpu/scripts/ao_gendisp.pl
ao_gendisp.pl: $(AO_GENDISP_PL)
	$(RM) $@; $(SYMLINK) $(AO_GENDISP_PL) $@

clean clobber:
	$(RM) ao_gendisp.pl $(PERLFILES)

include ../../../Makefile.targ
