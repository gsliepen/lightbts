#!/bin/sh

# Paths to executables

lbts=$PWD/../src/lbts

# Test directories

scriptname=`basename $0`
rm -rf $PWD/$scriptname.dir
mkdir $PWD/$scriptname.dir
cd $PWD/$scriptname.dir

# Exit on errors, log all commands being executed

set -ex
