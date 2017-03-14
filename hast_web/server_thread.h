#ifndef server_thread_h
#define server_thread_h
#include <mutex>
#include <thread>

class server_thread{
protected:
	short int i,j, alive_socket {0}, alive_thread{0};
	short int max_amount {0};
	short int recv_thread {-1};
	std::mutex recv_mx;

	std::vector<bool> in_execution;
	std::vector<std::thread*> thread_list;

	inline void get_thread();
	inline void resize();
	inline void add_thread();
public:
	std::function<void(const short int)> execute;
	std::vector<int> socketfd;
	std::vector< std::basic_string<unsigned char> > raw_msg;
	~server_thread();
};

#include <hast_web/server_thread.cpp>
#endif
