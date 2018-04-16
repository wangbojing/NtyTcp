
![image](https://github.com/wangbojing/NtyTcp/blob/master/doc/icon.jpg)

# netmap install
 
$ git clone https://github.com/wangbojing/netmap.git

$ ./configure

$ make 

$ sudo make install

# netmap install complete.

1. problem : configure --> /bin/sh^M. 

	you should run . 
	
	$ dos2unix configure
	
	$ dos2unix ./LINUX/configure


# NtyTcp
netmap, dpdp, pf_ring, Tcp Stack for Userspace 

compile:

$ sudo apt-get install libhugetlbfs-dev

$ make

update NtyTcp/include/nty_config.h  

#define NTY_SELF_IP		"192.168.0.106" 	//your ip

#define NTY_SELF_IP_HEX	0x6A00A8C0 			//your ip hex.

#define NTY_SELF_MAC	"00:0c:29:58:6f:f4" //your mac

block server run:

$ ./bin/nty_example_block_server

epoll server run:

$ ./bin/nty_example_epoll_rb_server




if you discover bug to sending email to 1989wangbojing@163.com. 

also, want to be an NtyTcper, so you can sent email to 1989wangbojing@163.com .

