#!/bin/sh

BUILD_DIR="../build"
if [ "$1" = "" ]; then
    NUM_THREADS=1
else
    NUM_THREADS=$1
fi

if [ -d "$BUILD_DIR" ]; then
    rm -rf $BUILD_DIR
fi

mkdir $BUILD_DIR && cd $BUILD_DIR || exit 1
cmake .. || exit 1
make -j "$NUM_THREADS"
sudo make install
