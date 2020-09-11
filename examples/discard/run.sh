#!/bin/bash

PLATFORM=$(uname -s)
CXXFLAGS="-std=c++17 -Wall -O2"

if [ "$PLATFORM" == "Darwin" ]; then
    CXX=clang++
elif [ "$PLATFORM" == "Linux" ]; then
    CXX=g++
    CXXFLAGS="$CXXFLAGS -lpthread"
fi

$CXX $CXXFLAGS discard-server.cc -langel -o server
$CXX $CXXFLAGS discard-client.cc -langel -o client
