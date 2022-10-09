#!/bin/bash

LSHOWCOLOR="\033[32m"
RSHOWCOLOR="\033[0m"

have_known_file=false
mime_types_file="./mime.types"

# If these known files not exist,
# we install "$mime_types_file" to "/usr/local/etc/".
known_files=(
    "/etc/mime.types"
    "/etc/httpd/mime.types"
    "/etc/httpd/conf/mime.types"
    "/etc/apache/mime.types"
    "/etc/apache2/mime.types"
    "/usr/local/etc/httpd/conf/mime.types"
    "/usr/local/lib/netscape/mime.types"
    "/usr/local/etc/mime.types"
)

for file in "${known_files[@]}"
do
    if [ ! -f "$file" ]; then
        continue
    fi
    have_known_file=true
    break
done

if [ $have_known_file == false ]; then
    if [ ! -f "$mime_types_file" ]; then
        echo -e "$LSHOWCOLOR Build failed, can't find $mime_types_file $RSHOWCOLOR"
        exit 1;
    fi
    cp "$mime_types_file" "/usr/local/etc/"
fi

echo -e "$LSHOWCOLOR Creating a directory ./build to build angel $RSHOWCOLOR"
mkdir ./build # ignore error "File exists"
cd ./build || return
if ! cmake .. || ! make -j4 install ; then
    echo -e "$LSHOWCOLOR Build failed $RSHOWCOLOR"
    exit 1;
fi
echo -e "$LSHOWCOLOR #################### $RSHOWCOLOR"
echo -e "$LSHOWCOLOR angel is built, now you can delete directory ./build $RSHOWCOLOR"
