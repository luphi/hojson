CC := gcc
CFLAGS := -I.. -g -s -Wall -ansi

ifeq ($(OS),Windows_NT)
	EXEC := hojson-test.exe
else
	EXEC := hojson-test.bin
endif

.PHONY: clean all

all: hojson-test.c
	$(CC) $(CFLAGS) hojson-test.c -o $(EXEC)

clean:
	rm -f $(EXEC)