#!/bin/bash

PLATFORM=$(uname -s)
CXXFLAGS="-std=c++11 -Wall -O2"

if [ "$PLATFORM" == "Darwin" ]; then
    CXX=clang++
    CXXFLAGS="$CXXFLAGS -Wno-unused-private-field"
elif [ "$PLATFORM" == "Linux" ]; then
    CXX=g++
    CXXFLAGS="$CXXFLAGS -lpthread"
fi

$CXX $CXXFLAGS EchoServer.cc -langel -o server
$CXX $CXXFLAGS EchoClient.cc -langel -o client
