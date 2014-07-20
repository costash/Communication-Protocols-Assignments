CC=g++
LIBSOCKET=-lnsl
CCFLAGS=-Wall -g
SRV=server
SEL_SRV=selectserver
CLT=client

all: $(SEL_SRV) $(CLT)

$(SEL_SRV):$(SEL_SRV).cpp
	$(CC) -o $(SEL_SRV) $(LIBSOCKET) $(CCFLAGS) $(SEL_SRV).cpp

$(CLT):	$(CLT).cpp
	$(CC) -o $(CLT) $(LIBSOCKET) $(CCFLAGS) $(CLT).cpp

clean:
	rm -f *.o *~
	rm -f $(SEL_SRV) $(CLT)