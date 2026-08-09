#!/bin/sh
cmake -S . -B build
cmake --build build -j$(nproc)
./build/mimita

