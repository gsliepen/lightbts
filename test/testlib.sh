#!/bin/sh

# Paths to executables

lbts=$PWD/src/lbts

# Test directories

scriptname=`basename $0`
rm -rf test/$scriptname.dir
mkdir test/$scriptname.dir
cd test/$scriptname.dir

# Exit on errors, log all commands being executed

set -ex

# Runs a command that must fail. Unlike "! command", this also works with set -e.

fails() {
	if "$@"; then
		echo "Unexpected success: $*" >&2
		exit 1
	fi
}
