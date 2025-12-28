#!/bin/bash

function format_files()
{
    for f in $1
    do
        if [ -f "$f" ]; then
            clang-format -i "$f"
        fi
    done
}

format_files "src/*.c"
format_files "src/include/*.h"
format_files "src/libhrmp/*.c"
