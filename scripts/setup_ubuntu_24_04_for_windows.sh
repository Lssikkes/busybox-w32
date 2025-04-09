#!/bin/bash

is_sourced() {
  # $0 is the name of the shell or script being executed
  # $BASH_SOURCE[0] is the name of the current script file
  [[ "${BASH_SOURCE[0]}" != "${0}" ]]
}

if is_sourced; then
 SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
 export PATH="$PATH:$SCRIPT_DIR/../toolchain/bin"
 [ "$CROSS_COMPILE" == "" ] && export CROSS_COMPILE=x86_64-w64-mingw32-
 [ "$CROSS_COMPILER" == "" ] && export CROSS_COMPILER=clang
else
 sudo apt-get install -y xz-utils

 if [ ! -d ../toolchain ]; then
  [ -f toolchain.tar.xz ] || wget https://github.com/mstorsjo/llvm-mingw/releases/download/20250402/llvm-mingw-20250402-ucrt-ubuntu-20.04-x86_64.tar.xz -O toolchain.tar.xz
  tar xf toolchain.tar.xz
  mv llvm-mingw-20250402-ucrt-ubuntu-20.04-x86_64 ../toolchain
 fi
fi
