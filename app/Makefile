
UBDIRS = $(shell ls -l | grep ^d | awk '{print $$9}')

CUR_SOURCE = ${wildcard *.c}

CUR_OBJS = ${patsubst %.c, %.o, $(CUR_SOURCE)}

all : $(SUB_DIRS) $(CUR_OBJS)

$(SUB_DIRS) : ECHO
	make -C %@

$(CUR_OBJS) : %.o:%.c
	$(CC) -c $^ -o $(BIN_DIR)/$@ $(FLAG)

ECHO : 
	@echo $(SUB_DIRS)

