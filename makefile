.POSIX:
.PHONY: all clean

TARGET = write_gpt
CC = gcc -D _POSIX_C_SOURCE=200809L
#CC = clang
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -O2 

all: $(TARGET)

clean:
	rm -f $(TARGET) *.img *.INF *.vhd
