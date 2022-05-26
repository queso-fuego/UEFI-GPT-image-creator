@echo off

set CC=gcc
set CFLAGS=-Wall -Werror -Wextra -Wpedantic -std=c17 -O2 -s
set SOURCE1=write_gpt.c
set TARGET1=write_gpt

%CC% %CFLAGS% %SOURCE1% -o %TARGET1%
