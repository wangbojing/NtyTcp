
CC = gcc
BIN = nty_stack
FLAG = -lpthread -lhugetlbfs -g

OBJS = *.o


all : 
	$(CC) -c *.c
	$(CC) -o $(BIN) *.o $(FLAG) 
clean :
	rm -rf *.o $(BIN)
