#ifndef ws_server_h
#define ws_server_h
#include <hast_web/socket_server.h>

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

class ws_server : public socket_server{
protected:
	WebSocketFrameType getFrame(unsigned char* in_buffer, int in_length, unsigned char* out_buffer, int out_size, int* out_length);
	int makeFrameU(WebSocketFrameType frame_type, unsigned char* msg, int msg_len, unsigned char* buffer, int buffer_len);
	int makeFrame(WebSocketFrameType frame_type, const char* msg, int msg_len, char* buffer, int buffer_len);
public:
	ws_server():
	socket_server(){
		reset_addr(hast::tcp_socket::SERVER);
	}
	std::string extract_from_raw(const short int thread_index);
	inline void echo_back_msg(const int socket_index, std::string &msg);
	inline void echo_back_msg(const int socket_index, const char *msg);
};

#include <hast_web/ws_server.cpp>
#endif
