.POSIX:
.PHONY: all clean

CC = cc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -O2 -s

TARGET = write_gpt

all: $(TARGET)

clean:
	rm -f $(TARGET) *.img *.inf *.vhd
