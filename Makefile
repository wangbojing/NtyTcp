
CC = gcc
BIN = nty_stack
FLAG = -lpthread -lhugetlbfs -g

OBJS = *.o


all : 
	$(CC) -c *.c -W -Wall -Wpointer-arith -Wno-unused-parameter -Werror
	$(CC) -o $(BIN) *.o $(FLAG) 
clean :
	rm -rf *.o $(BIN)
