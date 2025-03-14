#!/bin/ksh -p
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

#
# Copyright (c) 2012, 2016 by Delphix. All rights reserved.
#

. $STF_SUITE/include/libtest.shlib
. $STF_SUITE/tests/functional/cli_root/zpool_create/zpool_create.shlib

#
# DESCRIPTION:
# 'zpool create -f <pool> <vspec> ...' can successfully create a
# new pool in some cases.
#
# STRATEGY:
# 1. Prepare the scenarios for '-f' option
# 2. Use -f to override the devices to create new pools
# 3. Verify the pool created successfully
#

verify_runnable "global"

function cleanup
{
	for pool in $TESTPOOL $TESTPOOL1 $TESTPOOL2 $TESTPOOL3 $TESTPOOL4 \
		$TESTPOOL5 $TESTPOOL6
	do
		poolexists $pool && destroy_pool $pool
	done

	clean_blockfile "$TESTDIR0 $TESTDIR1"

	for file in /var/tmp/$FILEDISK0 /var/tmp/$FILEDISK1 /var/tmp/$FILEDISK2
	do
		if [[ -e $file ]]; then
			rm -rf $file
		fi
	done

	partition_disk $SIZE $disk 6
}

log_onexit cleanup

log_assert "'zpool create -f <pool> <vspec> ...' can successfully create" \
	"a new pool in some cases."

if [[ -n $DISK ]]; then
	disk=$DISK
else
	disk=$DISK0
fi
create_pool "$TESTPOOL" "${disk}s${SLICE0}"
log_must eval "new_fs ${DEV_RDSKDIR}/${disk}s${SLICE1} >/dev/null 2>&1"
create_blockfile $FILESIZE $TESTDIR0/$FILEDISK0 ${disk}s${SLICE4}
create_blockfile $FILESIZE1 $TESTDIR1/$FILEDISK1 ${disk}s${SLICE5}
log_must mkfile $SIZE /var/tmp/$FILEDISK0
log_must mkfile $SIZE /var/tmp/$FILEDISK1
log_must mkfile $SIZE /var/tmp/$FILEDISK2

unset NOINUSE_CHECK
log_must zpool export $TESTPOOL
log_note "'zpool create' without '-f' will fail " \
	"while device is belong to an exported pool."
log_mustnot zpool create "$TESTPOOL1" "${disk}s${SLICE0}"
create_pool "$TESTPOOL1" "${disk}s${SLICE0}"
log_must poolexists $TESTPOOL1

log_note "'zpool create' without '-f' will fail " \
	"while device is using by an ufs filesystem."
log_mustnot zpool create "$TESTPOOL2" "${disk}s${SLICE1}"
create_pool "$TESTPOOL2" "${disk}s${SLICE1}"
log_must poolexists $TESTPOOL2

log_note "'zpool create' mirror without '-f' will fail " \
	"while devices have different size."
log_mustnot zpool create "$TESTPOOL3" "mirror" $TESTDIR0/$FILEDISK0 $TESTDIR1/$FILEDISK1
create_pool "$TESTPOOL3" "mirror" $TESTDIR0/$FILEDISK0 $TESTDIR1/$FILEDISK1
log_must poolexists $TESTPOOL3

log_note "'zpool create' mirror without '-f' will fail " \
	"while devices are of different types."
log_mustnot zpool create "$TESTPOOL4" "mirror" /var/tmp/$FILEDISK0 \
	${disk}s${SLICE3}
create_pool "$TESTPOOL4" "mirror" /var/tmp/$FILEDISK0 ${disk}s${SLICE3}
log_must poolexists $TESTPOOL4

log_note "'zpool create' without '-f' will fail " \
	"while device is part of potentially active pool."
create_pool "$TESTPOOL5"  "mirror" /var/tmp/$FILEDISK1 \
	/var/tmp/$FILEDISK2
log_must zpool offline $TESTPOOL5 /var/tmp/$FILEDISK2
log_must zpool export $TESTPOOL5
log_mustnot zpool create "$TESTPOOL6" /var/tmp/$FILEDISK2
create_pool $TESTPOOL6 /var/tmp/$FILEDISK2
log_must poolexists $TESTPOOL6

log_pass "'zpool create -f <pool> <vspec> ...' success."
