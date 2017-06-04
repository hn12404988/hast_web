#ifndef hast_web_wss_server_hpp
#define hast_web_wss_server_hpp
#include <hast_web/socket_server.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <map>

class wss_server : public hast_web::socket_server{
protected:
	std::mutex ssl_mx;
	SSL_CTX *ctx {nullptr};
	SSL **ssl {nullptr};
	std::map<int,SSL*> ssl_map;
	void reset_accept(int socket_index,SSL *ssl = nullptr);
	void close_socket(int socket) override;
	inline void recv_epoll() override;
	bool read_loop(const short int thread_index, std::basic_string<unsigned char> &raw_str) override;
	WebSocketFrameType pop_pending(const short int thread_index) override;
	inline bool write(SSL *ssl_ptr,char cmsg[], std::size_t len);
public:
	std::function<bool(SSL *ssl, std::string &user, std::string &password)> on_open {nullptr};
	std::function<bool(SSL *ssl, std::string &user, std::string &password)> on_connect {nullptr};
	~wss_server();
	void start_accept();
	bool init(const char* crt, const char* key, hast::tcp_socket::port port, short int unsigned max = 2);
	void close_socket(const short int thread_index) override;
	inline void echo_back_msg(const short int thread_index, std::string &msg);
	inline void echo_back_msg(const short int thread_index, const char *msg);
	inline void echo_back_blob(const short int thread_index, std::vector<char> &blob);
	inline void echo_back_msg(SSL *ssl, std::string &msg);
	inline void echo_back_msg(SSL *ssl, const char *msg);
	inline void echo_back_blob(SSL *ssl, std::vector<char> &blob);
};

#include <hast_web/wss_server.cpp>
#endif /* hast_web_wss_server_hpp */
