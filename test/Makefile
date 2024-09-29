CC:=gcc
CFLAGS:=-I.. -O1 -s -Wall -std=c89

ifeq ($(OS),Windows_NT)
	EXEC:=hojson-test.exe
else
	EXEC:=hojson-test.bin
endif

.PHONY: clean all

all:
	$(CC) $(CFLAGS) hojson-test.c -o $(EXEC)

clean:
	rm -f $(EXEC)