#ifndef hast_web_tcp_config_h
#define hast_web_tcp_config_h

#include<sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <string>
#include <vector>
#include<unistd.h>    //close
#include <sys/epoll.h>

//#include<arpa/inet.h> //inet_addr
//#include <netinet/in.h>

/******************** Error Flag *****************************
 * 1: Server doesn't exist, or socket has problem.
 * 2: Fail on sending message.
 * 3: Server's execution crash.
 * 4: No reply.
 * 5: 'forget_msg_list' or 'priority_msg_list' is blocked.
 * 6: Fail on epoll.
 * 7: Invalid message format.
 *************************************************************/

namespace hast{
	namespace tcp_socket{
		typedef std::string ip;
		typedef std::string port;
		const bool SERVER {true};
		const bool CLIENT {false};
	}
}
class tcp_config{
private:
protected:
	struct addrinfo hints, *res{nullptr};
	inline void reset_addr(bool server_or_client);
};
#include<hast_web/tcp_config.cpp>
#endif
