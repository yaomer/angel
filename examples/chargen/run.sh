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

$CXX $CXXFLAGS ChargenServer.cc -langel -o server
$CXX $CXXFLAGS ChargenClient.cc -langel -o client
