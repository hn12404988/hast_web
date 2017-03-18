void wss_server::start_accept(){
	if(execute==nullptr){
		return;
	}
	SSL *ssl {nullptr};
	ssl = SSL_new(ctx);
	int l;
	char new_char[transport_size];
	std::string msg;
	struct pollfd ufds;
	ufds.events = POLLIN;
	int new_socket {1};
	while(new_socket>=0){
		new_socket = accept4(host_socket, (struct sockaddr *)&client_addr, &client_addr_size, SOCK_NONBLOCK);
		if(new_socket>0){
			msg.clear();
			ufds.fd = new_socket;
			if(poll(&ufds,1,3000)<=0 || (ufds.revents & POLLIN)==false){
				shutdown(new_socket,SHUT_RDWR);
				close(new_socket);
				continue;
			}
			l = SSL_set_fd(ssl, new_socket);
			std::cout << "set: " << l << std::endl;
			l = SSL_accept(ssl);
			std::cout << "accept: " << l << std::endl;
			if (l <= 0) {
				l = SSL_get_error(ssl,l);
				ERR_print_errors_fp(stderr);
				continue;
			}
			for(;;){
				l = SSL_read(ssl, new_char, transport_size);
				std::cout << "read: " << l << std::endl;
				//l = recv(new_socket, new_char, transport_size, 0);
				if(l>0){
					l += msg.length();
					msg.append(new_char);
					msg.resize(l);
					l = 0;
				}
				else{
					l = SSL_get_error(ssl,l);
					std::cout << "read error: " << l << std::endl;
					std::cout << "error queue: " << ERR_get_error()  << std::endl;
					break;
				}
			}
			std::cout << "msg: " << msg << std::endl;
			if(msg==""){
				shutdown(new_socket,SHUT_RDWR);
				close(new_socket);
				continue;
			}
			upgrade(msg);
			if(msg==""){
				shutdown(new_socket,SHUT_RDWR);
				close(new_socket);
				continue;
			}
			ev.data.fd = new_socket;
			if(epoll_ctl(epollfd, EPOLL_CTL_ADD, new_socket,&ev)==-1){
				shutdown(new_socket,SHUT_RDWR);
				close(new_socket);
				continue;
			}
			SSL_write(ssl, msg.c_str(), msg.length());
			if(on_open!=nullptr){
				on_open(ssl);
			}
			if(recv_thread==-1){
				add_thread();
			}
			++alive_socket;
		}
	}
	SSL_free(ssl);
}

bool wss_server::init(const char* crt, const char* key, hast::tcp_socket::port port, short int unsigned max){
	if(socket_server::init(port, max)==false){
		return false;
	}
	return server_thread::wss_init(crt,key);
}

inline int wss_server::get_socket_fd(const short int thread_index){
	return SSL_get_fd(socketfd[thread_index]);
}

inline int wss_server::get_socket_fd(SSL *ssl){
	return SSL_get_fd(ssl);
}

inline void wss_server::echo_back_msg(const short int thread_index, std::string &msg){
	int len {msg.length()+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME,msg.c_str(),len-1,buf,len);
	SSL_write(socketfd[thread_index], buf, len);
}

inline void wss_server::echo_back_msg(const short int thread_index, const char* msg){
	int len {strlen(msg)+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME, msg, len-1,buf,len);
	SSL_write(socketfd[thread_index], buf, len);
}

inline void wss_server::echo_back_msg(SSL *ssl, std::string &msg){
	int len {msg.length()+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME,msg.c_str(),len-1,buf,len);
	SSL_write(ssl, buf, len);
}

inline void wss_server::echo_back_msg(SSL *ssl, const char* msg){
	int len {strlen(msg)+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME, msg, len-1,buf,len);
	SSL_write(ssl, buf, len);
}
