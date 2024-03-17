PROJECT=c-uipc
VERSION=1.0.0
PREFIX=/usr/local
PROGS=usock_test_srv$(EXE) usock_test_cli$(EXE)
CC=gcc -Wall -std=c99
LIBS=-lpthread

all: $(PROGS)
clean:
	rm -f $(PROGS)
install:

usock_test_srv$(EXE): usock_test_srv.c usock.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) 
usock_test_cli$(EXE): usock_test_cli.c usock.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) 
