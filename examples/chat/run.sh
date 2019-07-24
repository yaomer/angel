#!/bin/bash

PLATFORM=$(uname -s)
CXXFLAGS="-std=c++17 -Wall -O2"

if [ "$PLATFORM" == "Darwin" ]; then
    CXX=clang++
    CXXFLAGS="$CXXFLAGS -Wno-unused-private-field"
elif [ "$PLATFORM" == "Linux" ]; then
    CXX=g++
    CXXFLAGS="$CXXFLAGS -lpthread"
fi

$CXX $CXXFLAGS ChatServer.cc -langel -o server
