#!/bin/bash

PLATFORM=$(uname -s)
CXXFLAGS="-std=c++11 -Wall -Wno-unused-private-field"

if [ "$PLATFORM" == "Darwin" ]; then
    CXX=clang++
elif [ "$PLATFORM" == "Linux" ]; then
    CXX=g++
fi

$CXX $CXXFLAGS ChatServer.cc -langel -o server
