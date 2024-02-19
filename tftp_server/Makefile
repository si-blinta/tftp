CC := gcc
CFLAGS := -I../include
LDFLAGS := -Wall
SRC := server.c ../include/utils.c
SERVER_BIN := server

.PHONY: all clean

all:
	$(CC) $(CFLAGS) $(SRC) -o $(SERVER_BIN) $(LDFLAGS)

clean:
	rm -f $(SERVER_BIN)
