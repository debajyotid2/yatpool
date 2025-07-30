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

set -e
mkdir $BUILD_DIR && cd $BUILD_DIR
cmake ..
cmake --build . -j "$NUM_THREADS"
