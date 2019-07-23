#!/bin/bash

LSHOWCOLOR="\033[32m"
RSHOWCOLOR="\033[0m"

echo -e "$LSHOWCOLOR Creating a directory ./build to build Angel $RSHOWCOLOR"
mkdir ./build && cd ./build || return
if ! cmake .. || ! make install ; then
    echo -e "$LSHOWCOLOR Build failed $RSHOWCOLOR"
    return 1;
fi
echo -e "$LSHOWCOLOR Angel is built
 libangel.a -> /usr/local/lib
 header files -> /usr/local/include/Angel
 now you can delete directory ./build $RSHOWCOLOR"
