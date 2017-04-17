#ifndef hast_web_server_thread_h
#define hast_web_server_thread_h
#include <mutex>
#include <thread>
#include <map>
#include <hast_web/crypto.hpp>

namespace hast_web{
	const char WAIT {0};
	const char BUSY {1};
	const char RECYCLE {2};
	const char READ {3};
	const char READ_PREFIX {4};
	const char GET {5};
	template<class sock_T>
	class server_thread{
	protected:
		server_thread();
		~server_thread();
		int alive_socket {0};
		short int max_amount {0}, alive_thread{0}, recv_thread {-1};
		std::mutex thread_mx;
		std::map<int,sock_T> *ssl_map {nullptr};

		std::vector<char> status;
		std::vector<std::thread*> thread_list;

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
};
#include <hast_web/server_thread.cpp>
#endif
