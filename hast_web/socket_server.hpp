#ifndef hast_web_socket_server_hpp
#define hast_web_socket_server_hpp
#include <hast/tcp_config.hpp>
#include <hast_web/crypto.hpp>
#include <hast_web/server_thread.hpp>
#include <sys/poll.h>
#include <cstring>
#include <list>

#define MAX_EVENTS 10
enum WebSocketFrameType {
	ERROR_FRAME=0xFF00,
	INCOMPLETE_FRAME=0xFE00,

	OPENING_FRAME=0x3300,
	CLOSING_FRAME=0x3400,

	DONE_TEXT=0x01,
	DONE_TEXT_BEHIND=0x02,
	DONE_TEXT_CONTIN=0x03,
	DONE_BINARY=0x04,
	DONE_BINARY_BEHIND=0x05,
	DONE_BINARY_CONTIN=0x06,
	CONTIN_TEXT=0x07,
	CONTIN_TEXT_BEHIND=0x08,
	CONTIN_BINARY=0x09,
	CONTIN_BINARY_BEHIND=0x0A,
	OVERSIZE_FRAME=0x0B,
	NO_MESSAGE=0x0C,
	RECYCLE_THREAD=0x0D,

	TEXT_FRAME=0x81,
	BINARY_FRAME=0x82,
	
	PING_FRAME=0x19,
	PONG_FRAME=0x1A
};

namespace hast_web{
	class socket_server : public tcp_config, public hast_web::server_thread{
	protected:
		struct sockaddr_storage client_addr;
		socklen_t client_addr_size;
		int epollfd;
		bool got_it {true};
		const int listen_pending{50};
		const int transport_size{100};
		const int resize_while_loop{20};
		struct epoll_event ev,ev_tmp, events[MAX_EVENTS];
		int host_socket {0};
		std::list<std::string> pending_msg;
		std::list<int> pending_socket;
		std::list<bool> pending_done;
	
		std::mutex wait_mx,close_mx;

		bool single_poll(const int socket_index, const short int time);
		inline void epoll_on(const short int thread_index);
		inline void clear_pending(int socket_index);
		/**
		 * RETURN NO_MESSAGE
		 * RETURN DONE_TEXT
		 * RETURN CONTIN_TEXTE
		 **/
		virtual WebSocketFrameType pop_pending(const short int thread_index);
		/**
		 * RETURN -1: No further action, kepp going.
		 * RETURN  0: Get msg, handle this request.
		 * RETURN >0: Get msg, and this socket has more msgs coming.
		 **/
		short int msg_pop_pending(const short int thread_index);
		std::string* push_pending(int socket_index, char *msg, bool done);
		void upgrade(std::string &headers,std::string &user,std::string &password);
		WebSocketFrameType more_data(const short int thread_index, short int &count);
		virtual bool read_loop(const short int thread_index, std::basic_string<unsigned char> &raw_str);
		virtual inline void recv_epoll();
		WebSocketFrameType getFrame(unsigned char* in_buffer, size_t in_length, unsigned char* out_buffer, size_t out_size, size_t* resize_length);
		int makeFrameU(WebSocketFrameType frame_type, unsigned char* msg, int msg_len, unsigned char* buffer, int buffer_len);
		int makeFrame(WebSocketFrameType frame_type, const char* msg, int msg_len, char* buffer, int buffer_len);
		virtual void close_socket(int socket);
	public:
		socket_server();
		~socket_server();
		std::function<void(const int)> on_close {nullptr};
		bool init(hast::tcp_socket::port port, short int unsigned max = 2);
		bool msg_recv(const short int thread_index);
		/**
		 * RETURN DONE_TEXT
		 * RETURN CONTIN_TEXT
		 * RETURN RECYCLE_THREAD
		 **/
		WebSocketFrameType partially_recv(const short int thread_index);
		/**
		 * RETURN DONE_TEXT
		 * RETURN CONTIN_TEXT
		 * RETURN NO_MESSAGE
		 * RETURN ERROR_FRAME
		 * RETURN DONE_TEXT_CONTIN
		 **/
		WebSocketFrameType more_recv(const short int thread_index);
		void close_socket(const short int thread_index);
		void done(const short int thread_index);
	};
};
#include <hast_web/socket_server.cpp>
#endif /* hast_web_socket_server_hpp */
