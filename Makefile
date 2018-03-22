
CC = gcc
#CC = /usr/bin/gcc-5
SUB_DIRS = app/ src/
BIN = nty_stack
FLAG = -lpthread -lhugetlbfs -W -Wall -Wpointer-arith -Wno-unused-parameter -Werror -I $(ROOT_DIR)/include

ROOT_DIR = $(shell pwd)
BIN_DIR = $(ROOT_DIR)/bin
OBJS_DIR = $(ROOT_DIR)/obj

CUR_SOURCE = ${wildcard *.c}
CUR_OBJS = ${patsubst %.c, %.o, $(CUR_SOURCE)}

export CC OBJS_DIR BIN_DIR OBJS_DIR FLAG

all : $(SUB_DIRS) $(CUR_OBJS) $(BIN_DIR)

$(SUB_DIRS) : ECHO
	make -C $@

$(BIN_DIR) : ECHO
	make -C $@

ECHO :
	@echo $(SUB_DIRS)

$(CUR_OBJS) : %.o : %.c
	$(CC) -c $^ -o $(ROOT_DIR)/$(OBJS_DIR)/$@

clean :
	rm -rf $(OBJS_DIR)/*
	rm -rf $(BIN_DIR)/*.o


