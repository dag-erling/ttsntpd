#!/bin/sh
#
# $Id: autogen.sh 136663 2008-02-12 15:17:03Z des $
#

set -e

aclocal
autoheader
automake --add-missing --copy --foreign
autoconf
