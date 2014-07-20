CFLAGS=-g 

all: send recv 

send: send.o lib.o crc.o
	gcc $(CFLAGS) crc.o send.o lib.o -o send

recv: recv.o lib.o crc.o
	gcc $(CFLAGS) -g crc.o recv.o lib.o -o recv

.c.o: 
	gcc $(CFLAGS) -Wall -c $? 

clean:
	-rm -f crc.o send.o recv.o send recv
