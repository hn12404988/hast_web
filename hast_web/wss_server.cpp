void wss_server::reset_accept(int socket_index,SSL *ssl){
	shutdown(socket_index,SHUT_RDWR);
	close(socket_index);
	SSL_free(ssl);
}

void wss_server::start_accept(){
	if(execute==nullptr || ctx==nullptr){
		return;
	}
	SSL *ssl {SSL_new(ctx)};
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
				reset_accept(new_socket);
				continue;
			}
			if(ssl==nullptr){
				ssl = SSL_new(ctx);
			}
			l = SSL_set_fd(ssl, new_socket);
			for(;;){
				l = SSL_accept(ssl);
				if (l <= 0) {
					l = SSL_get_error(ssl,l);
					if (l == SSL_ERROR_WANT_READ){
						//std::cout << "Wait for data to be read" << l << std::endl;
						continue;
					}
					else if (l == SSL_ERROR_WANT_WRITE){
						//std::cout << "Write data to continue" << l << std::endl;
					}
					else if (l == SSL_ERROR_SYSCALL){
						//std::cout << "SSL_ERROR_SYSCALL" << l << std::endl;
					}
					else if(l == SSL_ERROR_SSL){
						//std::cout << "SSL_ERROR_SSL" << l << std::endl;
					}
					else if (l == SSL_ERROR_ZERO_RETURN){
						//std::cout << "Same as error" << l << std::endl;
					}
					//ERR_print_errors_fp(stderr);
					reset_accept(new_socket,ssl);
					ssl = nullptr;
					l = -1;
					break;
				}
				else{
					break;
				}
			}
			if(l==-1){
				continue;
			}
			if(poll(&ufds,1,3000)<=0 || (ufds.revents & POLLIN)==false){
				reset_accept(new_socket,ssl);
				ssl = nullptr;
				continue;
			}
			for(;;){
				l = SSL_read(ssl, new_char, transport_size);
				if(l>0){
					l += msg.length();
					msg.append(new_char);
					msg.resize(l);
					l = 0;
				}
				else{
					/*
					l = SSL_get_error(ssl,l);
					if (l == SSL_ERROR_WANT_READ){
						std::cout << "Wait for data to be read" << l << std::endl;
					}
					else if (l == SSL_ERROR_WANT_WRITE){
						std::cout << "Write data to continue" << l << std::endl;
					}
					else if (l == SSL_ERROR_SYSCALL){
						std::cout << "SSL_ERROR_SYSCALL" << l << std::endl;
					}
					else if(l == SSL_ERROR_SSL){
						std::cout << "SSL_ERROR_SSL" << l << std::endl;
					}
					else if (l == SSL_ERROR_ZERO_RETURN){
						std::cout << "Same as error" << l << std::endl;
					}
					std::cout << "error queue: " << ERR_get_error()  << std::endl;
					*/
					break;
				}
			}
			if(msg==""){
				reset_accept(new_socket,ssl);
				ssl = nullptr;
				continue;
			}
			upgrade(msg);
			if(msg==""){
				reset_accept(new_socket,ssl);
				ssl = nullptr;
				continue;
			}
			ev.data.fd = new_socket;
			if(epoll_ctl(epollfd, EPOLL_CTL_ADD, new_socket,&ev)==-1){
				reset_accept(new_socket,ssl);
				ssl = nullptr;
				continue;
			}
			SSL_write(ssl, msg.c_str(), msg.length());
			if(on_open!=nullptr){
				on_open(ssl);
			}
			if((*ssl_map)[new_socket]!=nullptr){
				close_socket(new_socket,0);
				SSL_free(ssl);
				ssl = nullptr;
				continue;
			}
			else{
				(*ssl_map)[new_socket] = ssl;
				ssl = nullptr;
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
