#!/bin/bash

PLATFORM=$(uname -s)

if [ "$PLATFORM" == "Linux" ]; then
    PLATFORM="linux"
elif [ "$PLATFORM" == "Darwin" ]; then
    PLATFORM="macos"
else
    echo "Unknown Platform"
    exit 1
fi
echo "$PLATFORM"
exit 0
