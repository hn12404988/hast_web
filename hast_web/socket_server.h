#ifndef socket_server_h
#define socket_server_h
#include <hast_web/tcp_config.h>
#include <hast_web/server_thread.h>
#include <sys/poll.h>
#include <cstring>
#include <map>

#define MAX_EVENTS 10
enum WebSocketFrameType {
	ERROR_FRAME=0xFF00,
	INCOMPLETE_FRAME=0xFE00,

	OPENING_FRAME=0x3300,
	CLOSING_FRAME=0x3400,

	INCOMPLETE_TEXT_FRAME=0x01,
	INCOMPLETE_BINARY_FRAME=0x02,

	TEXT_FRAME=0x81,
	BINARY_FRAME=0x82,

	PING_FRAME=0x19,
	PONG_FRAME=0x1A
};

template<class sock_T>
class socket_server : public tcp_config, public server_thread<sock_T>{
protected:
	struct sockaddr_storage client_addr;
	socklen_t client_addr_size;
	int epollfd;
	bool got_it {true};
	const int listen_pending{50};
	const int transport_size{100};
	struct epoll_event ev,ev_tmp, events[MAX_EVENTS];
	int host_socket {0};
	
	std::mutex waiting_mx;
	
	void upgrade(std::string &headers);
	inline void close_socket(const int socket_index);
	inline void recv_epoll();
	WebSocketFrameType getFrame(unsigned char* in_buffer, int in_length, unsigned char* out_buffer, int out_size, int* out_length);
	int makeFrameU(WebSocketFrameType frame_type, unsigned char* msg, int msg_len, unsigned char* buffer, int buffer_len);
	int makeFrame(WebSocketFrameType frame_type, const char* msg, int msg_len, char* buffer, int buffer_len);
public:
	socket_server();
	~socket_server();
	std::function<void(const int)> on_close {nullptr};
	bool init(hast::tcp_socket::port port, short int unsigned max = 0);
	bool msg_recv(const short int thread_index);
	void done(const short int thread_index);
};

#include <hast_web/socket_server.cpp>
#endif
