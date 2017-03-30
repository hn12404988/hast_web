#ifndef ws_server_h
#define ws_server_h
#include <hast_web/socket_server.hpp>

class ws_server : public socket_server<int>{
public:
	std::function<void(const int)> on_open {nullptr};
	void start_accept();
	inline void echo_back_msg(const short int thread_index, std::string &msg);
	inline void echo_back_msg(const short int thread_index, const char *msg);
};

#include <hast_web/ws_server.cpp>
#endif
