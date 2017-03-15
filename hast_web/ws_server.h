#ifndef ws_server_h
#define ws_server_h
#include <hast_web/socket_server.h>

class ws_server : public socket_server{
public:
	ws_server():
	socket_server(){
		reset_addr(hast::tcp_socket::SERVER);
	}
	inline void echo_back_msg(const int socket_index, std::string &msg);
	inline void echo_back_msg(const int socket_index, const char *msg);
};

#include <hast_web/ws_server.cpp>
#endif
