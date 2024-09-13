#!/bin/sh

BUILD_DIR="../build"
if [ "$1" = "" ]; then
    NUM_THREADS=1
else
    NUM_THREADS=$1
fi

mkdir $BUILD_DIR && cd $BUILD_DIR || exit 1
cmake .. || exit 1
cmake --build . -j "$NUM_THREADS"
