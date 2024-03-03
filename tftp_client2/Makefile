CC := gcc
CFLAGS := -I../include
LDFLAGS := -Wall
SRC := client.c ../include/utils.c
client_BIN := client

.PHONY: all clean

all:
	$(CC) $(CFLAGS) $(SRC) -o $(client_BIN) $(LDFLAGS)

clean:
	rm -f $(client_BIN)
