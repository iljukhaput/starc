#!/bin/sh

#
# TODO: 
# Add keys for architecture i386 or amd64
# Add version update availability
# Rename result file to choosing version and architecture
#

#
# $1 - application version
#

#
# Prepare folders structure
#
EXE_APP_DIR="files_64"
#
# Copy files for MSIX packaging
#
ROOT_DIR="../../"
mkdir $EXE_APP_DIR/Images
cp $ROOT_DIR/Images/* $EXE_APP_DIR/Images
cp $ROOT_DIR/AppxManifest.xml $EXE_APP_DIR
