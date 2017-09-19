#!/usr/bin/env bash
file=`basename $1`
fileName=`basename $1 .cpp`
g++ -std=c++11 -I ./include/ -L ./ $file -o $fileName  -lmozjs-52 -lz -lpthread -ldl
#if $2; then
    chmod +x ./$fileName
    `echo ./$fileName`
#fi
