#!/usr/bin/bash

current_dir="$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd external/cmph && autoreconf --install --make && ./configure --prefix="${current_dir}/external/cmph/build/" && make && make install && cd ../../
