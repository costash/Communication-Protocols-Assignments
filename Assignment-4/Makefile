TARGET=my_dns_client
SRC=my_dns_client.cpp
CC=g++
CFLAGS=-g -Wall

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS)
	
clean:
	rm -rf $(TARGET)