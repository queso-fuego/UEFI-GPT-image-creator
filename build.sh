#!/bin/sh

set -eu

CC="cc"
CFLAGS="-Wall -Werror -Wextra -Wpedantic -std=c17 -O2 -s"
SOURCE1="write_gpt.c"
TARGET1="write_gpt"

$CC $CFLAGS $SOURCE1 -o $TARGET1

