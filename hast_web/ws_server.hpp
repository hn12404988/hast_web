#ifndef hast_web_ws_server_h
#define hast_web_ws_server_h
#include <hast_web/socket_server.hpp>

class ws_server : public hast_web::socket_server<int>{
public:
	std::function<void(const int)> on_open {nullptr};
	void start_accept();
	inline void echo_back_msg(const short int thread_index, std::string &msg);
	inline void echo_back_msg(const short int thread_index, const char *msg);
	inline void echo_back_msg(const int socket_index, std::string &msg);
	inline void echo_back_msg(const int socket_index, const char *msg);
};

#include <hast_web/ws_server.cpp>
#endif
