.POSIX:
.PHONY: all clean

TARGET = write_gpt
CC = gcc
#CC = clang
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -O2 

all: $(TARGET)

clean:
	rm -f $(TARGET)

