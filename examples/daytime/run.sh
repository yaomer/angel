#!/bin/bash

PLATFORM=$(uname -s)
CXXFLAGS="-std=c++11 -Wall"

if [ "$PLATFORM" == "Darwin" ]; then
    CXX=clang++
elif [ "$PLATFORM" == "Linux" ]; then
    CXX=g++
fi

$CXX $CXXFLAGS DaytimeServer.cc -L ../../ -langel -o server
$CXX $CXXFLAGS DaytimeClient.cc -L ../../ -langel -o client
