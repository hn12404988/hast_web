#ifndef hast_web_server_thread_hpp
#define hast_web_server_thread_hpp
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
		short int max_thread {0}, recv_thread {-1};
		std::mutex thread_mx;
		std::map<int,sock_T> *ssl_map {nullptr};

		char *status {nullptr};
		std::thread **thread_list {nullptr};

		SSL_CTX *ctx {nullptr};
		void destruct();
		void init();
		bool wss_init(const char* crt, const char*key);
		short int get_thread();
		short int get_thread_no_recv();
		inline void resize(short int amount);
		inline void add_thread();
	public:
		std::function<void(const short int)> execute {nullptr};
		sock_T *socketfd {nullptr};
		std::string *raw_msg {nullptr};
	};
};
#include <hast_web/server_thread.cpp>
#endif /* hast_web_server_thread_hpp */
