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
