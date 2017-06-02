wss_server::~wss_server(){
	std::map<int,SSL*>::iterator it;
	std::map<int,SSL*>::iterator it_end;
	it = ssl_map.begin();
	it_end = ssl_map.end();
	for(;it!=it_end;++it){
		if(it->second!=nullptr){
			SSL_free(it->second);
			it->second = nullptr;
		}
	}
	ssl_map.clear();
	if(ctx!=nullptr){
		SSL_CTX_free(ctx);
		ctx = nullptr;
	}
	//CONF_modules_unload(1);
	//CONF_modules_free();
	//ENGINE_cleanup();
	sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_state(0);
	ERR_free_strings();
	if(ssl!=nullptr){
		delete [] ssl;
		ssl = nullptr;
	}
}

void wss_server::reset_accept(int socket_index,SSL *ssl_ptr){
	shutdown(socket_index,SHUT_RDWR);
	close(socket_index);
	ssl_mx.lock();
	SSL_free(ssl_ptr);
	ssl_mx.unlock();
}

void wss_server::start_accept(){
	if(execute==nullptr || ctx==nullptr){
		return;
	}
	SSL *ssl_ptr {SSL_new(ctx)};
	int l;
	char new_char[transport_size];
	std::string msg,user,password;
	int new_socket {1};
	while(new_socket>=0){
		new_socket = accept4(host_socket, (struct sockaddr *)&client_addr, &client_addr_size, SOCK_NONBLOCK);
		if(new_socket>0){
			msg.clear();
			if(single_poll(new_socket,3000)==false){
				shutdown(new_socket,SHUT_RDWR);
				close(new_socket);
				continue;
			}
			if(ssl_ptr==nullptr){
				ssl_mx.lock();
				ssl_ptr = SSL_new(ctx);
				ssl_mx.unlock();
			}
			ssl_mx.lock();
			l = SSL_set_fd(ssl_ptr, new_socket);
			for(;;){
				l = SSL_accept(ssl_ptr);
				if (l <= 0) {
					l = SSL_get_error(ssl_ptr,l);
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
					ssl_mx.unlock();
					reset_accept(new_socket,ssl_ptr);
					ssl_mx.lock();
					ssl_ptr = nullptr;
					l = -1;
					break;
				}
				else{
					break;
				}
			}
			ssl_mx.unlock();
			if(l==-1){
				continue;
			}
			if(single_poll(new_socket,3000)==false){
				reset_accept(new_socket,ssl_ptr);
				ssl_ptr = nullptr;
				continue;
			}
			ssl_mx.lock();
			for(;;){
				l = SSL_read(ssl_ptr, new_char, transport_size);
				if(l>0){
					msg.append(new_char,l);
				}
				else{
					/*
					  l = SSL_get_error(ssl_ptr,l);
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
			ssl_mx.unlock();
			if(msg==""){
				reset_accept(new_socket,ssl_ptr);
				ssl_ptr = nullptr;
				continue;
			}
			upgrade(msg,user,password);
			if(msg==""){
				reset_accept(new_socket,ssl_ptr);
				ssl_ptr = nullptr;
				continue;
			}
			if(on_connect!=nullptr){
				if(on_connect(ssl_ptr,user,password)==false){
					reset_accept(new_socket,ssl_ptr);
					ssl_ptr = nullptr;
					continue;
				}
			}
			write(ssl_ptr,msg);
			if(on_open!=nullptr){
				if(on_open(ssl_ptr,user,password)==false){
					reset_accept(new_socket,ssl_ptr);
					ssl_ptr = nullptr;
					continue;
				}
			}
			if(ssl_map[new_socket]!=nullptr){
				if(on_close!=nullptr){
					on_close(new_socket);
				}
				reset_accept(new_socket,ssl_ptr);
				ssl_ptr = nullptr;
				ssl_map[new_socket] = nullptr;
				continue;
			}
			else{
				ssl_map[new_socket] = ssl_ptr;
			}
			ev.data.fd = new_socket;
			if(epoll_ctl(epollfd, EPOLL_CTL_ADD, new_socket,&ev)==-1){
				reset_accept(new_socket,ssl_ptr);
				ssl_ptr = nullptr;
				ssl_map[new_socket] = nullptr;
				continue;
			}
			ssl_ptr = nullptr;
			if(recv_thread==-1){
				add_thread();
			}
		}
	}
	if(ssl_ptr!=nullptr){
		ssl_mx.lock();
		SSL_free(ssl_ptr);
		ssl_mx.unlock();
	}
}

bool wss_server::init(const char* crt, const char* key, hast::tcp_socket::port port, short int unsigned max){
	if(hast_web::socket_server::init(port, max)==false){
		return false;
	}
	if(ssl==nullptr){
		ssl = new SSL* [max_thread];
	}
	const SSL_METHOD *method;
	SSL_load_error_strings();	
	OpenSSL_add_ssl_algorithms();
	SSL_library_init();
	method = SSLv23_server_method();
	ctx = SSL_CTX_new(method);
	if (!ctx) {
		perror("Unable to create SSL context");
		ERR_print_errors_fp(stderr);
		return false;
	}
	//SSL_CTX_set_ecdh_auto(ctx, 1); This is removed in newer openssl.
	/* Set the key and cert */
	if (SSL_CTX_use_certificate_file(ctx, crt, SSL_FILETYPE_PEM) < 0) {
		ERR_print_errors_fp(stderr);
		return false;
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) < 0 ) {
		ERR_print_errors_fp(stderr);
		return false;
	}
	return true;
}

inline void wss_server::echo_back_msg(const short int thread_index, std::string &msg){
	int len {msg.length()+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME,msg.c_str(),len-1,buf,len);
	buf[len] = '\0';
	msg = buf;
	write(socketfd[thread_index],msg);
}

inline void wss_server::echo_back_msg(const short int thread_index, const char* msg){
	int len {strlen(msg)+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME, msg, len-1,buf,len);
	buf[len] = '\0';
	std::string tmp_msg(buf);
	write(socketfd[thread_index],tmp_msg);
}

inline void wss_server::echo_back_msg(SSL *ssl_ptr, std::string &msg){
	//Use this method only in on_open
	if(ssl==nullptr){
		return;
	}
	int len {msg.length()+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME,msg.c_str(),len-1,buf,len);
	buf[len] = '\0';
	msg = buf;
	write(ssl_ptr,msg);
}

inline void wss_server::echo_back_msg(SSL *ssl_ptr, const char* msg){
	//Use this method only in on_open
	if(ssl==nullptr){
		return;
	}
	int len {strlen(msg)+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME, msg, len-1,buf,len);
	buf[len] = '\0';
	std::string tmp_msg(buf);
	write(ssl_ptr,tmp_msg);
}

void wss_server::close_socket(const short int thread_index){
	close_socket(socketfd[thread_index]);
}

void wss_server::close_socket(int socket){
	ssl_mx.lock();
	socket_server::close_socket(socket);
	SSL* tmp_ssl {nullptr};
	tmp_ssl = ssl_map[socket];
	if(tmp_ssl==nullptr){
		ssl_mx.unlock();
		return;
	}
	short int a;
	SSL_free(tmp_ssl);
	for(a=0;a<max_thread;++a){
		if(ssl[a]==tmp_ssl){
			ssl[a] = nullptr;
		}
	}
	ssl_map[socket] = nullptr;
	ssl_mx.unlock();
}

inline void wss_server::recv_epoll(){
	short int a,b,wait_amount {0};
	int c,loop_amount {0};
	while(got_it==false){}
	for(;;){
		b = 0;
		wait_amount = 0;
		for(;b<max_thread;++b){
			if(status[recv_thread]==hast_web::READ_PREFIX){
				break;
			}
			else if(status[b]==hast_web::READ){
				--b;
				continue;
			}
			else if(status[b]==hast_web::WAIT){
				++wait_amount;
			}
		}
		if(b<max_thread){
			break;
		}
		wait_mx.lock();
		if(wait_amount>1){
			++loop_amount;
			if(loop_amount>resize_while_loop){
				resize(wait_amount-1);
			}
			else{
				resize(0);
			}
		}
		else{
			resize(0);
			loop_amount = 0;
		}
		for(;;){
			a = epoll_wait(epollfd, events, MAX_EVENTS, 3500);
			if(a>0){
				wait_mx.unlock();
				break;
			}
		}
		--a;
		for(;a>=0;--a){
			c = events[a].data.fd;
			if(events[a].events!=1){
				close_socket(c);
				continue;
			}
			b = get_thread();
			if(b==-1){
				break;
			}
			ssl_mx.lock();
			ssl[b] = ssl_map[c];
			if(ssl[b]==nullptr){
				status[b] = hast_web::WAIT;
				ssl_mx.unlock();
				socket_server::close_socket(c);
				continue;
			}
			else{
				ssl_mx.unlock();
			}
			got_it = false;
			status[b] = hast_web::READ;
			socketfd[b] = c;
			ev_tmp.data.fd = c;
			epoll_ctl(epollfd, EPOLL_CTL_MOD, c,&ev_tmp);
			if(b!=recv_thread){
				while(got_it==false){}
			}
		}
		if(a>=0){
			for(;a>=0;--a){
				add_thread();
			}
			break;
		}
		else{
			if(b==recv_thread){
				add_thread();
				break;
			}
		}
	}
	resize(0);
	recv_thread = -1;
}

bool wss_server::read_loop(const short int thread_index, std::basic_string<unsigned char> &raw_str){
	ssl_mx.lock();
	SSL *tmp {ssl[thread_index]};
	ssl_mx.unlock();
	if(tmp==nullptr){
		return false;
	}
	int a = SSL_get_fd(tmp);
	if(a==-1){
		return false;
	}
	if(single_poll(a,3000)==false){
		return true;
	}
	unsigned char new_char[transport_size];
	for(;;){
		ssl_mx.lock();
		a = SSL_read(tmp, new_char, transport_size);
		ssl_mx.unlock();
		if(a>0){
			raw_str.append(new_char,a);
		}
		else{
			a = SSL_get_error(tmp,a);
			if (a == SSL_ERROR_WANT_READ || a == SSL_ERROR_WANT_WRITE){
				return true;
			}
			else{
				return false;
			}
			break;
		}
	}
}

WebSocketFrameType wss_server::pop_pending(const short int thread_index){
	close_mx.lock();
	if(pending_done.size()==0){
		close_mx.unlock();
		return NO_MESSAGE;
	}
	else{
		bool done;
		raw_msg[thread_index].clear();
		/*
		  while(raw_msg[thread_index]==""){
		  raw_msg[thread_index].assign(pending_msg.front());
		  }
		*/
		raw_msg[thread_index] = pending_msg.front();
		pending_msg.pop_front();
		socketfd[thread_index] = pending_socket.front();
		pending_socket.pop_front();
		done = pending_done.front();
		pending_done.pop_front();
		//Add this line
		ssl[thread_index] = ssl_map[socketfd[thread_index]];
		close_mx.unlock();
		if(done==true){
			return DONE_TEXT;
		}
		else{
			return CONTIN_TEXT;
		}
	}
}

inline bool wss_server::write(SSL *ssl_ptr,std::string &msg){
	if(ssl_ptr==nullptr){
		return false;
	}
	int fd;
	fd = SSL_get_fd(ssl_ptr);
	if(fd==-1){
		return false;
	}
	int len {msg.length()},flag;
	const char* cmsg {msg.c_str()};
	ssl_mx.lock();
	for(;;){
		flag = SSL_write(ssl_ptr, cmsg, len);
		if(flag>0){
			if(flag==len){
				ssl_mx.unlock();
				return true;
			}
			else{
				msg = msg.substr(flag);
				cmsg = msg.c_str();
				len = msg.length();
			}
		}
		else if(flag==0){
			ssl_mx.unlock();
			close_socket(fd);
			return false;
		}
		else{
			flag = SSL_get_error(ssl_ptr,flag);
			if(flag==SSL_ERROR_WANT_READ || flag==SSL_ERROR_WANT_WRITE){
				continue;
			}
			else{
				ssl_mx.unlock();
				close_socket(fd);
				return false;
			}
		}
	}
}

inline bool wss_server::write(int socket_index,std::string &msg){
	if(socket_index<0){
		return false;
	}
	SSL *ssl_ptr {ssl_map[socket_index]};
	if(ssl_ptr==nullptr){
		return false;
	}
	int len {msg.length()},flag;
	const char* cmsg {msg.c_str()};
	ssl_mx.lock();
	for(;;){
		flag = SSL_write(ssl_ptr, cmsg, len);
		if(flag>0){
			if(flag==len){
				ssl_mx.unlock();
				return true;
			}
			else{
				msg = msg.substr(flag);
				cmsg = msg.c_str();
				len = msg.length();
			}
		}
		else if(flag==0){
			ssl_mx.unlock();
			close_socket(socket_index);
			return false;
		}
		else{
			flag = SSL_get_error(ssl_ptr,flag);
			if(flag==SSL_ERROR_WANT_READ || flag==SSL_ERROR_WANT_WRITE){
				continue;
			}
			else{
				ssl_mx.unlock();
				close_socket(socket_index);
				return false;
			}
		}
	}
}
