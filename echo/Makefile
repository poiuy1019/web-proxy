# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread -lrt

all: echo

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

echo.o: echo.c
	$(CC) $(CFLAGS) -c echo.c

echo: echo.o echo_client.c echo_server.c csapp.o
	$(CC) $(CFLAGS) echo_client.c csapp.o -o echo_client $(LDFLAGS)
	$(CC) $(CFLAGS) echo_server.c echo.o csapp.o -o echo_server $(LDFLAGS)

clean:
	rm -f *~ *.o echo_client echo_server 
