#!/bin/bash

PLATFORM=$(uname -s)
CXXFLAGS="-std=c++11 -Wall"

if [ "$PLATFORM" == "Darwin" ]; then
    CXX=clang++
elif [ "$PLATFORM" == "Linux" ]; then
    CXX=g++
fi

$CXX $CXXFLAGS *.cc -langel -o server
