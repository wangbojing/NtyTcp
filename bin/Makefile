
BLOCK_SERVER_OBJ = $(BIN_DIR)/nty_example_tcp_server.o
EPOLL_SERVER_OBJ = $(BIN_DIR)/nty_example_epoll_server.o
EPOLL_RB_SERVER_OBJ = $(BIN_DIR)/nty_example_epoll_rb_server.o

BLOCK_SERVER = nty_example_block_server
EPOLL_SERVER = nty_example_epoll_server
EPOLL_RB_SERVER = nty_example_epoll_rb_server

all : $(BLOCK_SERVER) $(EPOLL_SERVER) $(EPOLL_RB_SERVER)

$(BLOCK_SERVER) : $(BLOCK_SERVER_OBJ)
	$(CC) -o $(BIN_DIR)/$(BLOCK_SERVER) $(OBJS_DIR)/* $(BLOCK_SERVER_OBJ) $(FLAG)

$(EPOLL_SERVER) : $(EPOLL_SERVER_OBJ)
	$(CC) -o $(BIN_DIR)/$(EPOLL_SERVER) $(OBJS_DIR)/* $(EPOLL_SERVER_OBJ) $(FLAG)

$(EPOLL_RB_SERVER) : $(EPOLL_RB_SERVER_OBJ)
	$(CC) -o $(BIN_DIR)/$(EPOLL_RB_SERVER) $(OBJS_DIR)/* $(EPOLL_RB_SERVER_OBJ) $(FLAG)


