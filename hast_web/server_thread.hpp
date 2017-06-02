#ifndef hast_web_server_thread_hpp
#define hast_web_server_thread_hpp
#include <mutex>
#include <thread>
#include <map>

namespace hast_web{
	const char WAIT {0};
	const char BUSY {1};
	const char RECYCLE {2};
	const char READ {3};
	const char READ_PREFIX {4};
	const char GET {5};
	
	class server_thread{
	protected:
		server_thread();
		~server_thread();
		short int max_thread {0}, recv_thread {-1};
		std::mutex thread_mx;

		char *status {nullptr};
		std::thread **thread_list {nullptr};

		void init();
		short int get_thread();
		short int get_thread_no_recv();
		inline void resize(short int amount);
		inline void add_thread();
	public:
		std::function<void(const short int)> execute {nullptr};
		std::string *raw_msg {nullptr};
		int *socketfd {nullptr};
	};
};
#include <hast_web/server_thread.cpp>
#endif /* hast_web_server_thread_hpp */
