#ifndef hast_web_wss_server_h
#define hast_web_wss_server_h
#include <hast_web/socket_server.hpp>

class wss_server : public hast_web::::socket_server<SSL*>{
protected:
	void reset_accept(int socket_index,SSL *ssl = nullptr);
public:
	std::function<bool(SSL *ssl, std::string &user, std::string &password)> on_open {nullptr};
	std::function<bool(SSL *ssl, std::string &user, std::string &password)> on_connect {nullptr};
	void start_accept();
	bool init(const char* crt, const char* key, hast::tcp_socket::port port, short int unsigned max = 0);
	inline int get_socket_fd(const short int thread_index);
	inline int get_socket_fd(SSL *ssl);
	inline void echo_back_msg(const short int thread_index, std::string &msg);
	inline void echo_back_msg(const short int thread_index, const char *msg);
	inline void echo_back_msg(SSL *ssl, std::string &msg);
	inline void echo_back_msg(SSL *ssl, const char *msg);
};

#include <hast_web/wss_server.cpp>
#endif
