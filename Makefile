# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall -std=gnu99
LIBS = 

OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o

all:   myClient myServer

myClient: cclient.c $(OBJS)
	$(CC) $(CFLAGS) -o myClient cclient.c  $(OBJS) $(LIBS)

myServer: server.c $(OBJS)
	$(CC) $(CFLAGS) -o myServer server.c $(OBJS) $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f myServer myClient *.o




