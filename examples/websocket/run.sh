#!/bin/bash

PLATFORM=$(uname -s)
CXXFLAGS="-std=c++17 -Wall -O2"

if [ "$PLATFORM" == "Darwin" ]; then
    CXX=clang++
elif [ "$PLATFORM" == "Linux" ]; then
    CXX=g++
    CXXFLAGS="$CXXFLAGS -lpthread"
fi

$CXX $CXXFLAGS sha1.cc base64.cc ws-server.cc -langel -o server
