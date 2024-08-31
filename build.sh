#!/bin/sh

set -eu

CC="cc"
CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -O2"
SOURCE="write_gpt.c"
TARGET="write_gpt"

$CC $CFLAGS $SOURCE -o $TARGET

