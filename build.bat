@echo off

set CC=gcc
set CFLAGS=-std=c17 -Wall -Wextra -Wpedantic -O2 -s
set SOURCE=write_gpt.c
set TARGET=write_gpt

%CC% %CFLAGS% %SOURCE% -o %TARGET%
