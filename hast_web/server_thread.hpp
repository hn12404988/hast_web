#ifndef hast_web_server_thread_h
#define hast_web_server_thread_h
#include <mutex>
#include <thread>
#include <map>
#include <hast_web/crypto.hpp>

template<class sock_T>
class server_thread{
protected:
	server_thread();
	~server_thread();
	int alive_socket {0};
	short int max_amount {0}, alive_thread{0}, recv_thread {-1};
	std::mutex recv_mx;
	std::map<int,sock_T> *ssl_map {nullptr};

	/**
	 * @in_execution [0] : waiting for request
	 * @in_execution [1] : processing request
	 * @in_execution [2] : ready to be recycled
	 **/
	std::vector<char> in_execution;
	std::vector<std::thread*> thread_list;
	std::vector<bool> pending_done;

	SSL_CTX *ctx {nullptr};
	bool wss_init(const char* crt, const char*key);
	short int get_thread();
	inline void resize();
	inline void add_thread();
public:
	std::function<void(const short int)> execute {nullptr};
	std::vector<sock_T> socketfd;
	std::vector<std::string> raw_msg;
};

#include <hast_web/server_thread.cpp>
#endif
