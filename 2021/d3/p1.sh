#!/bin/bash
cd $(dirname "${BASH_SOURCE[0]}")

gcc p1.c -o p1
./p1 "$@"
rm p1