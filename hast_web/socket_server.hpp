#ifndef socket_server_h
#define socket_server_h
#include <hast_web/tcp_config.hpp>
#include <hast_web/server_thread.hpp>
#include <sys/poll.h>
#include <cstring>
#include <map>
#include <list>

#define MAX_EVENTS 10
enum WebSocketFrameType {
	ERROR_FRAME=0xFF00,
	INCOMPLETE_FRAME=0xFE00,

	OPENING_FRAME=0x3300,
	CLOSING_FRAME=0x3400,

	DONE_TEXT=0x01,
	DONE_TEXT_BEHIND=0x02,
	DONE_BINARY=0x03,
	DONE_BINARY_BEHIND=0x04,
	CONTIN_TEXT=0x05,
	CONTIN_TEXT_BEHIND=0x06,
	CONTIN_BINARY=0x07,
	CONTIN_BINARY_BEHIND=0x08,
	OVERSIZE_FRAME=0x09,
	NO_MESSAGE=0x0A,
	RECYCLE_THREAD=0x0B,

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
	std::map<short int,std::list<std::string>* > pending;
	
	std::mutex waiting_mx;

	bool single_poll(const int socket_index, const short int time);
	void clear_pending(const short int thread_index);
	bool pop_pending(const short int thread_index);
	void upgrade(std::string &headers);
	void reset_recv(const short int thread_index);
	WebSocketFrameType more_data(const short int thread_index);
	bool read_loop(const short int thread_index, std::basic_string<unsigned char> &raw_str);
	inline void recv_epoll();
	void close_socket(const short int thread_index);
	WebSocketFrameType getFrame(unsigned char* in_buffer, size_t in_length, unsigned char* out_buffer, size_t out_size, size_t* resize_length);
	int makeFrameU(WebSocketFrameType frame_type, unsigned char* msg, int msg_len, unsigned char* buffer, int buffer_len);
	int makeFrame(WebSocketFrameType frame_type, const char* msg, int msg_len, char* buffer, int buffer_len);
public:
	socket_server();
	~socket_server();
	std::function<void(const int)> on_close {nullptr};
	bool init(hast::tcp_socket::port port, short int unsigned max = 0);
	bool msg_recv(const short int thread_index);
	/**
	 * RETURN DONE_TEXT
	 * RETURN CONTIN_TEXT
	 * RETURN RECYCLE_THREAD
	 **/
	WebSocketFrameType partially_recv(const short int thread_index);
	/**
	 * `more_recv` only works with `partially_recv`. Try to call `more_recv` under `msg_recv` will cause fatal problems.
	 * RETURN DONE_TEXT
	 * RETURN CONTIN_TEXT
	 * RETURN NO_MESSAGE
	 * RETURN ERROR_FRAME
	 **/
	WebSocketFrameType more_recv(const short int thread_index);
	void done(const short int thread_index);
};

#include <hast_web/socket_server.cpp>
#endif
