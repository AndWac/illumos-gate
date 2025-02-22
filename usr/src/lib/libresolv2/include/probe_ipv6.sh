#!/bin/sh

# Copyright 2003 by Sun Microsystems, Inc. All rights reserved.
# Use is subject to license terms.
#

set -e
PATH=/bin:/usr/bin:$PATH; export PATH
trap "rm -f tmp$$[abc].[oc]" 0
target=port_ipv6
new=new_${target}.h
old=${target}.h

cat > tmp$$a.c <<EOF
#include <sys/types.h>
#include <netinet/in.h>
struct sockaddr_in6 xx;
EOF

cat > tmp$$b.c <<EOF
#include <sys/types.h>
#include <netinet/in.h>
struct in6_addr xx;
EOF

cat > tmp$$c.c <<EOF
#include <sys/types.h>
#include <netinet/in.h>

struct sockaddr_in6 xx;
int
main() { xx.sin6_scope_id = 0; }
EOF

cat > ${new} <<EOF

/* This file is automatically generated. Do Not Edit. */

#ifndef ${target}_h
#define ${target}_h

EOF

if ${CC} ${CPPFLAGS} -c tmp$$a.c > /dev/null 2>&1
then
        echo "#define HAS_INET6_STRUCTS" >> ${new}
        if ${CC} ${CPPFLAGS} -c tmp$$b.c > /dev/null 2>&1
        then
		:
	else
		printf "build failed to detect struct in6_addr\n" >&2
		exit 1
        fi
	if ${CC} ${CPPFLAGS} -c tmp$$c.c > /dev/null 2>&1
	then
		echo "#define HAVE_SIN6_SCOPE_ID" >> ${new}
	else
		printf "build failed to detect sin6_scope_id\n" >&2
		exit 1
	fi
else
	printf "build failed to detect struct sockaddr_in6\n" >&2
	exit 1
fi
echo  >> ${new}
echo "#endif" >> ${new}
if [ -f ${old} ]; then
        if cmp -s ${new} ${old} ; then
                rm -f ${new}
        else
                rm -f ${old}
                mv ${new} ${old}
        fi
else
        mv ${new} ${old}
fi
exit 0
