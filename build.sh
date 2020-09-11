#!/bin/bash

LSHOWCOLOR="\033[32m"
RSHOWCOLOR="\033[0m"

echo -e "$LSHOWCOLOR Creating a directory ./build to build angel $RSHOWCOLOR"
if [ -d "./build" ]; then
    rm -fr ./build;
fi
mkdir ./build && cd ./build || return
if ! cmake .. || ! make install ; then
    echo -e "$LSHOWCOLOR Build failed $RSHOWCOLOR"
    exit 1;
fi
echo -e "$LSHOWCOLOR angel is built
 libangel.a -> /usr/local/lib
 header files -> /usr/local/include/angel
 now you can delete directory ./build $RSHOWCOLOR"
