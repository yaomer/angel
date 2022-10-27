#!/bin/bash

LCOLOR="\033[32m"
RCOLOR="\033[0m"

install_prefix=
build_type=
use_ssl=

for arg in "$@"
do
    if [ $arg == "--with-ssl" ]; then
        use_ssl="-DANGEL_USE_OPENSSL=ON"
    elif [ ${arg:0:17} == "--install-prefix=" ]; then
        install_prefix="-DCMAKE_INSTALL_PREFIX=${arg:17}"
    elif [ ${arg:0:9} == "--release" ]; then
        build_type="-DCMAKE_BUILD_TYPE=Release"
    elif [ ${arg:0:7} == "--debug" ]; then
        build_type="-DCMAKE_BUILD_TYPE=Debug"
    else
        echo -e "illegal option -- '$arg'"
        exit 1;
    fi
done

cmake_args="$install_prefix $build_type $use_ssl"

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
        echo -e "$LCOLOR Build failed, can't find $mime_types_file $RCOLOR"
        exit 1;
    fi
    cp "$mime_types_file" "/usr/local/etc/"
fi

echo -e "$LCOLOR Creating a directory ./build to build angel $RCOLOR"
mkdir ./build # ignore error "File exists"
cd ./build || return
if ! cmake $cmake_args .. || ! make -j4 install ; then
    echo -e "$LCOLOR Build failed $RCOLOR"
    exit 1;
fi
echo -e "$LCOLOR #################### $RCOLOR"
echo -e "$LCOLOR angel is built, now you can delete directory ./build $RCOLOR"
