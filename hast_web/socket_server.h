#ifndef socket_server_h
#define socket_server_h
#include <hast_web/tcp_config.h>
#include <hast_web/server_thread.h>
#include <hast_web/crypto.h>
#include <sys/poll.h>
#include <cstring>
#include <map>

#define MAX_EVENTS 10

class socket_server : public tcp_config, public server_thread{
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

	inline void close_socket(const int socket_index);
	inline void recv_epoll();
	void upgrade(std::string &headers);
public:
	socket_server();
	~socket_server();
	bool init(hast::tcp_socket::port port, short int unsigned max);
	bool msg_recv(const short int thread_index);
	void start_accept();
	void done(const short int thread_index);
	int get_socket(short int thread_index);
};

#include <hast_web/socket_server.cpp>
#endif
