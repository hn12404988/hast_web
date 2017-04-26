namespace hast_web{
	template<class sock_T>
	socket_server<sock_T>::socket_server(){
		reset_addr(hast::tcp_socket::SERVER);
		client_addr_size = sizeof(client_addr);
		epollfd = epoll_create1(0);
		ev.events = EPOLLIN;
		ev_tmp.events = EPOLLHUP;
	}

	template<class sock_T>
	socket_server<sock_T>::~socket_server(){
		close(epollfd);
	}

	template<class sock_T>
	bool socket_server<sock_T>::single_poll(const int socket_index, const short int time){
		struct pollfd ufds;
		ufds.events = POLLIN;
		ufds.fd = socket_index;
		if(poll(&ufds,1,time)<=0 || (ufds.revents & POLLIN)==false){
			return false;
		}
		return true;
	}

	template<>
	void socket_server<int>::close_socket(const short int thread_index, const int line){
		close_mx.lock();
		int socket = socketfd[thread_index];
		short int a;
		if(socket<0){
			close_mx.unlock();
			return;
		}
		std::cout << "close: " << line << std::endl;
		if(on_close!=nullptr){
			on_close(socket);
		}
		epoll_ctl(epollfd, EPOLL_CTL_DEL, socket,nullptr);
		shutdown(socket,SHUT_RDWR);
		close(socket);
		a = socketfd.size()-1;
		for(;a>=0;--a){
			if(socketfd[a]==socket){
				socketfd[a] = -1;
			}
		}
		clear_pending(socket);
		close_mx.unlock();
	}

	template<>
	void socket_server<SSL*>::close_socket(const short int thread_index, const int line){
		close_mx.lock();
		SSL* tmp_ssl {socketfd[thread_index]};
		if(tmp_ssl==nullptr){
			close_mx.unlock();
			return;
		}
		int socket = SSL_get_fd(tmp_ssl);
		short int a;
		if(socket<0){
			close_mx.unlock();
			return;
		}
		std::cout << "close: " << line << std::endl;
		tmp_ssl = (*ssl_map)[socket];
		if(tmp_ssl!=nullptr){
			SSL_free(tmp_ssl);
			(*ssl_map)[socket] = nullptr;
		}
		if(on_close!=nullptr){
			on_close(socket);
		}
		epoll_ctl(epollfd, EPOLL_CTL_DEL, socket,nullptr);
		shutdown(socket,SHUT_RDWR);
		close(socket);
		a = socketfd.size()-1;
		for(;a>=0;--a){
			if(socketfd[a]==tmp_ssl){
				socketfd[a] = nullptr;
			}
		}
		clear_pending(tmp_ssl);
		close_mx.unlock();
	}
	template<class sock_T>
	void socket_server<sock_T>::close_socket(const short int thread_index){
		close_socket(thread_index,__LINE__);
	}

	template<class sock_T>
	void socket_server<sock_T>::upgrade(std::string &headers,std::string &user,std::string &password){
		std::string key,value;
		size_t msg_len {headers.length()};
		int a,b;
		user.clear();
		password.clear();
		for(size_t i=0;i<msg_len;++i){
			if(headers[i]=='\n'){
				if(i+1==msg_len){
					break;
				}
				if(headers[i-1]=='\r'){
					key = headers.substr(0,i-1);
				}
				else{
					key = headers.substr(0,i);
				}
				b = key.length();
				for(a=0;a<b;++a){
					if(key[a]==':' && key[a+1]==' '){
						value = key.substr(a+2);
						key = key.substr(0,a);
						break;
					}
				}
				if(a==b){
					if(key.substr(0,3)=="GET"){
						key = key.substr(5);
						b = key.length();
						for(a=0;a<b;++a){
							if(key[a]=='&'){
								++a;
								break;
							}
							user.push_back(key[a]);
						}
						for(;a<b;++a){
							if(key[a]==' '){
								break;
							}
							password.push_back(key[a]);
						}
					}
				}
				else{
					if(key=="Sec-WebSocket-Key"){
						value.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
						headers.clear();
						headers.append("HTTP/1.1 101 Switching Protocols\r\n");
						headers.append("Server: hast_ws_server\r\n");
						headers.append("Upgrade: websocket\r\n");
						headers.append("Connection: Upgrade\r\nSec-WebSocket-Accept: ");
						headers.append(Crypto::Base64::encode(Crypto::sha1(value)));
						headers.append("\r\n\r\n");
						return;
					}
				}
				headers = headers.substr(i+1);
				msg_len = headers.length();
				i = 0;
			}
		}
		headers.clear();
	}

	template<class sock_T>
	bool socket_server<sock_T>::init(hast::tcp_socket::port port, short int unsigned max){
		hast_web::server_thread<sock_T>::max_amount = max;
		if(getaddrinfo(NULL, port.c_str(), &hints, &res)!=0){
			return false;
		}
		struct addrinfo *p{NULL};
		int flag {1};
		for(p = res; p != NULL; p = p->ai_next) {
			if ((host_socket = socket(p->ai_family, p->ai_socktype,
									  p->ai_protocol)) == -1) {
				continue;
			}
			if (setsockopt(host_socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
				close(host_socket);
				continue;
			}
			if (bind(host_socket, p->ai_addr, p->ai_addrlen) == -1) {
				close(host_socket);
				continue;
			}
			break;
		}
		if(p==NULL){
			return false;
		}
		freeaddrinfo(res); 
		if(listen(host_socket,listen_pending)==0){
			return true;
		}
		else{
			return false;
		}
	}

	template<class sock_T>
	void socket_server<sock_T>::done(const short int thread_index){
		/**
		 * This is here for threads break msg_recv loop accidentally.
		 * Threads are only allowed to break msg_recv loop by get 'false' from it.
		 **/
		hast_web::server_thread<sock_T>::status[thread_index] = hast_web::RECYCLE;
	}

	template<>
	inline void socket_server<int>::recv_epoll(){
		short int a,b,wait_amount {0};
		int c,loop_amount {0};
		while(got_it==false){}
		for(;;){
			b = socketfd.size()-1;
			wait_amount = 0;
			for(;b>=0;--b){
				if(status[recv_thread]==hast_web::READ_PREFIX){
					break;
				}
				else if(status[b]==hast_web::READ){
					++b;
					continue;
				}
				else if(status[b]==hast_web::WAIT){
					++wait_amount;
				}
			}
			if(b>=0){
				std::cout << "recv read pre: " << recv_thread << std::endl;
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
					epoll_ctl(epollfd, EPOLL_CTL_DEL, c,nullptr);
					shutdown(c,SHUT_RDWR);
					close(c);
					if(on_close!=nullptr){
						on_close(c);
					}
					continue;
				}
				b = get_thread();
				if(b==-1){
					break;
				}
				got_it = false;
				socketfd[b] = c;
				status[b] = hast_web::READ;
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

	template<>
	inline void socket_server<SSL*>::recv_epoll(){
		short int a,b;
		int c;
		while(got_it==false){}
		for(;;){
			b = socketfd.size()-1;
			for(;b>=0;--b){
				if(status[recv_thread]==hast_web::READ_PREFIX){
					break;
				}
				else if(status[b]==hast_web::READ){
					++b;
					continue;
				}
			}
			if(b>=0){
				break;
			}
			wait_mx.lock();
			//resize();
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
					SSL* tmp_ssl;
					tmp_ssl = (*ssl_map)[c];
					if(tmp_ssl!=nullptr){
						SSL_free(tmp_ssl);
					}
					(*ssl_map)[c] = nullptr;
					epoll_ctl(epollfd, EPOLL_CTL_DEL, c,nullptr);
					shutdown(c,SHUT_RDWR);
					close(c);
					if(on_close!=nullptr){
						on_close(c);
					}
					continue;
				}
				b = get_thread();
				if(b==-1){
					break;
				}
				if((*ssl_map)[c]==nullptr){
					status[b] = hast_web::WAIT;
					epoll_ctl(epollfd, EPOLL_CTL_DEL, c,nullptr);
					shutdown(c,SHUT_RDWR);
					close(c);
					continue;
				}
				else{
					socketfd[b] = (*ssl_map)[c];
				}
				got_it = false;
				status[b] = hast_web::READ;
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
		recv_thread = -1;
	}

	template<>
	bool socket_server<int>::read_loop(const short int thread_index, std::basic_string<unsigned char> &raw_str){
		if(single_poll(socketfd[thread_index],3000)==false){
			return true;
		}
		unsigned char new_char[transport_size];
		int a;
		for(;;){
			a = recv(socketfd[thread_index], new_char, transport_size,0);
			if(a>0){
				raw_str.append(new_char,a);
			}
			else if(a==0){
				//client close
				return false;
			}
			else{
				if(errno==EAGAIN || errno==EWOULDBLOCK){
					return true;
				}
				else{
					return false;
				}
			}
		}
	}

	template<>
	bool socket_server<SSL*>::read_loop(const short int thread_index, std::basic_string<unsigned char> &raw_str){
		if(single_poll(SSL_get_fd(socketfd[thread_index]),3000)==false){
			return true;
		}
		unsigned char new_char[transport_size];
		int a;
		for(;;){
			a = SSL_read(socketfd[thread_index], new_char, transport_size);
			if(a>0){
				raw_str.append(new_char,a);
			}
			else{
				a = SSL_get_error(socketfd[thread_index],a);
				if (a == SSL_ERROR_WANT_READ){
					return true;
				}
				else{
					return false;
				}
				break;
			}
		}
	}

	template<class sock_T>
	WebSocketFrameType socket_server<sock_T>::more_data(const short int thread_index, short int &count){
		std::basic_string<unsigned char> raw_str;
		if(read_loop(thread_index,raw_str)==false){
			close_socket(thread_index,__LINE__);
			return ERROR_FRAME;
		}
		else{
			if(raw_str.length()==0){
				return NO_MESSAGE;
			}
		}
		int a;
		std::string *str {&hast_web::server_thread<sock_T>::raw_msg[thread_index]};
		size_t resize_len;
		resize_len = raw_str.length();
		unsigned char u_msg[resize_len];
		a = getFrame(&raw_str[0], resize_len, u_msg, resize_len, &resize_len);
		if(a==DONE_TEXT || a==DONE_TEXT_BEHIND || a==CONTIN_TEXT || a==CONTIN_TEXT_BEHIND){
			str->append(reinterpret_cast<char*>(u_msg));
		}
		else{
			close_socket(thread_index,__LINE__);
			return ERROR_FRAME;
		}
		if(a==DONE_TEXT_BEHIND || a==CONTIN_TEXT_BEHIND){
			bool last_is_CONTIN {(a==CONTIN_TEXT_BEHIND)};
			bool already_done {(a==DONE_TEXT_BEHIND)};
			for(;;){
				raw_str = raw_str.substr(resize_len);
				resize_len = raw_str.length();
				a = getFrame(&raw_str[0], resize_len, u_msg, resize_len, &resize_len);
				if(a==DONE_TEXT || a==DONE_TEXT_BEHIND){
					already_done = true;
					if(last_is_CONTIN==true){
						str->append(reinterpret_cast<char*>(u_msg));
					}
					else{
						str = push_pending(hast_web::server_thread<sock_T>::socketfd[thread_index],reinterpret_cast<char*>(u_msg),true);
						++count;
					}
					last_is_CONTIN = false;
					if(a==DONE_TEXT){
						break;
					}
				}
				else if(a==CONTIN_TEXT || a==CONTIN_TEXT_BEHIND){
					str->append(reinterpret_cast<char*>(u_msg));
					last_is_CONTIN = true;
					if(a==CONTIN_TEXT){
						break;
					}
				}
				else{
					close_socket(thread_index,__LINE__);
					return ERROR_FRAME;
				}
			}
			if(already_done==true){
				if(a==DONE_TEXT){
					return DONE_TEXT;
				}
				else{// a==CONTIN_TEXT
					return DONE_TEXT_CONTIN;
				}
			}
			else{
				return CONTIN_TEXT;
			}
		}
		/**
		 * return a;
		 **/
		else if(a==CONTIN_TEXT){
			return CONTIN_TEXT;
		}
		else{
			return DONE_TEXT;
		}
	}

	template<class sock_T>
	WebSocketFrameType socket_server<sock_T>::more_recv(const short int thread_index){
		hast_web::server_thread<sock_T>::raw_msg[thread_index].clear();
		//TODO Remove this useless short int.
		short int useless;
		return more_data(thread_index,useless);
	}
	
	template<>
	inline void socket_server<int>::epoll_on(const short int thread_index){
		if(socketfd[thread_index]>=0){
			ev.data.fd = socketfd[thread_index];
			std::cout << "EPOLL ON: " << ev.data.fd << std::endl;
			epoll_ctl(epollfd, EPOLL_CTL_MOD, socketfd[thread_index],&ev);
		}
	}

	template<>
	inline void socket_server<SSL*>::epoll_on(const short int thread_index){
		close_mx.lock();
		if(socketfd[thread_index]!=nullptr){
			int fd;
			fd = SSL_get_fd(socketfd[thread_index]);
			close_mx.unlock();
			if(fd>=0){
				ev.data.fd = fd;
				std::cout << "EPOLL ON: " << ev.data.fd << std::endl;
				epoll_ctl(epollfd, EPOLL_CTL_MOD, fd,&ev);
			}
		}
		else{
			close_mx.unlock();
		}
	}
	
	template<>
	inline void socket_server<int>::reset_recv(const short int thread_index){
		socketfd[thread_index] = -1;
	}

	template<>
	inline void socket_server<SSL*>::reset_recv(const short int thread_index){
		socketfd[thread_index] = nullptr;
	}

	template<class sock_T>
	WebSocketFrameType socket_server<sock_T>::pop_pending(const short int thread_index){
		close_mx.lock();
		if(pending_done.size()==0){
			close_mx.unlock();
			return NO_MESSAGE;
		}
		else{
			bool done;
			hast_web::server_thread<sock_T>::raw_msg[thread_index].clear();
			while(hast_web::server_thread<sock_T>::raw_msg[thread_index]==""){
				hast_web::server_thread<sock_T>::raw_msg[thread_index].assign(pending_msg.front());
			}
			pending_msg.pop_front();
			hast_web::server_thread<sock_T>::socketfd[thread_index] = pending_socket.front();
			pending_socket.pop_front();
			done = pending_done.front();
			pending_done.pop_front();
			close_mx.unlock();
			if(done==true){
				return DONE_TEXT;
			}
			else{
				return CONTIN_TEXT;
			}
		}
	}

	template<class sock_T>
	std::string* socket_server<sock_T>::push_pending(sock_T socket_index, char *msg, bool done){
		std::string *str {nullptr};
		close_mx.lock();
		pending_msg.push_back(msg);
		str = &(pending_msg.back());
		pending_socket.push_back(socket_index);
		pending_done.push_back(done);
		close_mx.unlock();
		return str;
	}

	template<class sock_T>
	short int socket_server<sock_T>::msg_pop_pending(const short int thread_index){
		int type;
		short int count {0};
		type = pop_pending(thread_index);
		if(type!=NO_MESSAGE){
			if(type==CONTIN_TEXT){
				for(;;){
					type = more_data(thread_index,count);
					if(type==CONTIN_TEXT){
						continue;
					}
					else{
						break;
					}
				}
				if(type==DONE_TEXT){
					epoll_on(thread_index);
					return 0;
				}
				else if(type==DONE_TEXT_CONTIN){
					return count;
				}
				else{
					close_socket(thread_index,__LINE__);
					return -1;
				}
			}
			else{
				return 0;
			}
		}
		else{
			return -1;
		}
	}
	
	template<class sock_T>
	bool socket_server<sock_T>::msg_recv(const short int thread_index){
		int type;
		short int a,count {0};
		count = msg_pop_pending(thread_index);
		std::cout << "msg_recv count 1: " << count << std::endl;
		if(count==0){
			return true;
		}
		else if(count>0){
			/*
			for(;count>0;--count){
				//BUG Thread is probably locked in wait_mx.
				//TODO Make recv_thread can take job from here.
				a = hast_web::server_thread<sock_T>::get_thread_no_recv();
				if(a==-1){
					hast_web::server_thread<sock_T>::add_thread();
					continue;
				}
				hast_web::server_thread<sock_T>::status[a] = hast_web::READ_PREFIX;
			}
			*/
			return true;
		}
		for(;;){
			hast_web::server_thread<sock_T>::raw_msg[thread_index].clear();
			hast_web::server_thread<sock_T>::thread_mx.lock();
			reset_recv(thread_index);
			hast_web::server_thread<sock_T>::status[thread_index] = hast_web::WAIT;
			if(hast_web::server_thread<sock_T>::recv_thread==-1){
				hast_web::server_thread<sock_T>::recv_thread = thread_index;
				hast_web::server_thread<sock_T>::thread_mx.unlock();
				std::cout << "recv thread: " << thread_index << std::endl;
				recv_epoll();
				std::cout << "recv leave: " << thread_index << std::endl;
			}
			else{
				hast_web::server_thread<sock_T>::thread_mx.unlock();
			}
			for(;;){
				if(hast_web::server_thread<sock_T>::status[thread_index]==hast_web::READ || hast_web::server_thread<sock_T>::status[thread_index]==hast_web::READ_PREFIX){
					break;
				}
				else if(hast_web::server_thread<sock_T>::status[thread_index]==hast_web::RECYCLE){
					return false;
				}
				else if(hast_web::server_thread<sock_T>::recv_thread==-1){
					if(hast_web::server_thread<sock_T>::status[thread_index]==hast_web::WAIT){
						break;
					}
				}
				else{
					wait_mx.lock();
					wait_mx.unlock();
				}
			}
			if(hast_web::server_thread<sock_T>::status[thread_index]==hast_web::WAIT){
				continue;
			}
			else if(hast_web::server_thread<sock_T>::status[thread_index]==hast_web::READ_PREFIX){
				count = msg_pop_pending(thread_index);
				std::cout << "msg_recv count 2: " << count << std::endl;
				if(count==0){
					hast_web::server_thread<sock_T>::status[thread_index] = hast_web::BUSY;
					return true;
				}
				else if(count>0){
					/*
					for(;count>0;--count){
						//BUG Thread is probably locked in wait_mx.
						//TODO Make recv_thread can take job from here.
						a = hast_web::server_thread<sock_T>::get_thread_no_recv();
						if(a==-1){
							hast_web::server_thread<sock_T>::add_thread();
							continue;
						}
						hast_web::server_thread<sock_T>::status[a] = hast_web::READ_PREFIX;
					}
					*/
					hast_web::server_thread<sock_T>::status[thread_index] = hast_web::BUSY;
					return true;
				}
				else{
					continue;
				}
			}
			got_it = true;
			count = 0;
			for(;;){
				type = more_data(thread_index,count);
				if(type==CONTIN_TEXT){
					continue;
				}
				else{
					break;
				}
			}
			if(type==ERROR_FRAME){
				continue;
			}
			else{
				if(type==NO_MESSAGE){
					close_socket(thread_index,__LINE__);
					continue;
				}
				else{
					if(hast_web::server_thread<sock_T>::raw_msg[thread_index].length()==0){
						close_socket(thread_index,__LINE__);
						continue;
					}
				}
			}
			std::cout << "msg_recv count 3: " << count << std::endl;
			if(type==DONE_TEXT_CONTIN){
				for(;count>0;--count){
					a = hast_web::server_thread<sock_T>::get_thread();
					std::cout << "msg_recv thread 3: " << a << std::endl;
					if(a==-1){
						hast_web::server_thread<sock_T>::add_thread();
						continue;
					}
					hast_web::server_thread<sock_T>::status[a] = hast_web::READ_PREFIX;
				}
			}
			else if(type==DONE_TEXT){
				epoll_on(thread_index);
				for(;count>0;--count){
					a = hast_web::server_thread<sock_T>::get_thread();
					std::cout << "msg_recv thread 4: " << a << std::endl;
					if(a==-1){
						hast_web::server_thread<sock_T>::add_thread();
						continue;
					}
					hast_web::server_thread<sock_T>::status[a] = hast_web::READ_PREFIX;
				}
			}
			hast_web::server_thread<sock_T>::status[thread_index] = hast_web::BUSY;
			return true;
		}
	}

	template<class sock_T>
	WebSocketFrameType socket_server<sock_T>::partially_recv(const short int thread_index){
		int type;
		type = pop_pending(thread_index);
		if(type!=NO_MESSAGE){
			if(type==DONE_TEXT){
				return DONE_TEXT;
			}
			else if(type==CONTIN_TEXT){
				return CONTIN_TEXT;
			}
			else{
				close_socket(thread_index,__LINE__);
			}
		}
		for(;;){
			hast_web::server_thread<sock_T>::raw_msg[thread_index].clear();
			hast_web::server_thread<sock_T>::thread_mx.lock();
			reset_recv(thread_index);
			epoll_on(thread_index);
			hast_web::server_thread<sock_T>::status[thread_index] = hast_web::WAIT;
			if(hast_web::server_thread<sock_T>::recv_thread==-1){
				hast_web::server_thread<sock_T>::recv_thread = thread_index;
				hast_web::server_thread<sock_T>::thread_mx.unlock();
				recv_epoll();
			}
			else{
				hast_web::server_thread<sock_T>::thread_mx.unlock();
			}
			for(;;){
				if(hast_web::server_thread<sock_T>::status[thread_index]==hast_web::READ){
					break;
				}
				else if(hast_web::server_thread<sock_T>::status[thread_index]==hast_web::RECYCLE){
					return RECYCLE_THREAD;
				}
				else if(hast_web::server_thread<sock_T>::recv_thread==-1){
					if(hast_web::server_thread<sock_T>::status[thread_index]==hast_web::WAIT){
						break;
					}
				}
				else{
					wait_mx.lock();
					wait_mx.unlock();
				}
			}
			if(hast_web::server_thread<sock_T>::status[thread_index]==hast_web::WAIT){
				continue;
			}
			got_it = true;
			//TODO Remove this useless short int.
			short int useless;
			type = more_data(thread_index,useless);
			if(type==ERROR_FRAME){
				continue;
			}
			else if(type==NO_MESSAGE){
				close_socket(thread_index,__LINE__);
				continue;
			}
			else{
				if(hast_web::server_thread<sock_T>::raw_msg[thread_index].length()==0){
					close_socket(thread_index,__LINE__);
					continue;
				}
			}
			if(type==DONE_TEXT || type==DONE_TEXT_CONTIN){
				hast_web::server_thread<sock_T>::status[thread_index] = hast_web::BUSY;
				return DONE_TEXT;
			}
			else if(type==CONTIN_TEXT){
				hast_web::server_thread<sock_T>::status[thread_index] = hast_web::BUSY;
				return CONTIN_TEXT;
			}
			else{
				close_socket(thread_index,__LINE__);
			}
		}
	}

	template<class sock_T>
	int socket_server<sock_T>::makeFrameU(WebSocketFrameType frame_type, unsigned char* msg, int msg_length, unsigned char* buffer, int buffer_size)
	{
		int pos = 0;
		int size = msg_length; 
		buffer[pos++] = (unsigned char)frame_type; // text frame

		if(size <= 125) {
			buffer[pos++] = size;
		}
		else if(size <= 65535) {
			buffer[pos++] = 126; //16 bit length follows
		
			buffer[pos++] = (size >> 8) & 0xFF; // leftmost first
			buffer[pos++] = size & 0xFF;
		}
		else { // >2^16-1 (65535)
			buffer[pos++] = 127; //64 bit length follows
		
			// write 8 bytes length (significant first)
		
			// since msg_length is int it can be no longer than 4 bytes = 2^32-1
			// padd zeroes for the first 4 bytes
			for(int i=3; i>=0; i--) {
				buffer[pos++] = 0;
			}
			// write the actual 32bit msg_length in the next 4 bytes
			for(int i=3; i>=0; i--) {
				buffer[pos++] = ((size >> 8*i) & 0xFF);
			}
		}
		memcpy((void*)(buffer+pos), msg, size);
		return (size+pos);
	}

	template<class sock_T>
	int socket_server<sock_T>::makeFrame(WebSocketFrameType frame_type, const char* msg, int msg_length, char* buffer, int buffer_size)
	{
		int pos = 0;
		int size = msg_length; 
		buffer[pos++] = (unsigned char)frame_type; // text frame

		if(size <= 125) {
			buffer[pos++] = size;
		}
		else if(size <= 65535) {
			buffer[pos++] = 126; //16 bit length follows
		
			buffer[pos++] = (size >> 8) & 0xFF; // leftmost first
			buffer[pos++] = size & 0xFF;
		}
		else { // >2^16-1 (65535)
			buffer[pos++] = 127; //64 bit length follows
		
			// write 8 bytes length (significant first)
		
			// since msg_length is int it can be no longer than 4 bytes = 2^32-1
			// padd zeroes for the first 4 bytes
			for(int i=3; i>=0; i--) {
				buffer[pos++] = 0;
			}
			// write the actual 32bit msg_length in the next 4 bytes
			for(int i=3; i>=0; i--) {
				buffer[pos++] = ((size >> 8*i) & 0xFF);
			}
		}
		memcpy((void*)(buffer+pos), msg, size);
		return (size+pos);
	}

	template<class sock_T>
	WebSocketFrameType socket_server<sock_T>::getFrame(unsigned char* in_buffer, size_t in_length, unsigned char* out_buffer, size_t out_size, size_t* resize_length)
	{
		//printf("getTextFrame()\n");
		if(in_length < 3) return ERROR_FRAME;

		unsigned char msg_opcode = in_buffer[0] & 0x0F;
		unsigned char msg_fin = (in_buffer[0] >> 7) & 0x01;
		unsigned char msg_masked = (in_buffer[1] >> 7) & 0x01;

		// *** message decoding 

		size_t payload_length = 0;
		short int pos = 2;
		short int length_field = in_buffer[1] & (~0x80);
		unsigned int mask = 0;

		//printf("IN:"); for(int i=0; i<20; i++) printf("%02x ",buffer[i]); printf("\n");

		if(length_field <= 125) {
			payload_length = length_field;
		}
		else if(length_field == 126) { //msglen is 16bit!
			//payload_length = in_buffer[2] + (in_buffer[3]<<8);
			short int tmp;
			tmp = (short int)(in_buffer[2]);
			tmp = (short int)(in_buffer[3]);
			payload_length = ((in_buffer[2]) << 8) | in_buffer[3];
			pos += 2;
		}
		else if(length_field == 127) { //msglen is 64bit!
			payload_length = ((in_buffer[2]) << 56) | ((in_buffer[3]) << 48) | ((in_buffer[4]) << 40) | ((in_buffer[5]) << 32) | ((in_buffer[6]) << 24) | ((in_buffer[7]) << 16) | ((in_buffer[8]) << 8) | in_buffer[9]; 
			pos += 8;
		}
		//printf("PAYLOAD_LEN: %08x\n", payload_length);
		if(in_length < payload_length+pos) {
			return INCOMPLETE_FRAME;
		}

		if(msg_masked) {
			mask = *((unsigned int*)(in_buffer+pos));
			//printf("MASK: %08x\n", mask);
			pos += 4;

			// unmask data:
			unsigned char* c = in_buffer+pos;
			for(size_t i=0; i<payload_length; i++) {
				c[i] = c[i] ^ ((unsigned char*)(&mask))[i%4];
			}
		}
	
		if(payload_length > out_size) {
			return OVERSIZE_FRAME;
			//It can be text, binary or continual message.
		}

		memcpy((void*)out_buffer, (void*)(in_buffer+pos), payload_length);
		out_buffer[payload_length] = 0;
		*resize_length = payload_length+pos;
		//printf("TEXT: %s\n", out_buffer);
		if(msg_opcode == 0x0){
			if(in_length>*resize_length){
				return (msg_fin)?DONE_TEXT_BEHIND:CONTIN_TEXT_BEHIND;
			}
			else{
				return (msg_fin)?DONE_TEXT:CONTIN_TEXT;
			}
		}
		else if(msg_opcode == 0x1){
			if(in_length>*resize_length){
				return (msg_fin)?DONE_TEXT_BEHIND:CONTIN_TEXT_BEHIND;
			}
			else{
				return (msg_fin)?DONE_TEXT:CONTIN_TEXT;
			}
		}
		else if(msg_opcode == 0x2){
			if(in_length>*resize_length){
				return (msg_fin)?DONE_BINARY_BEHIND:CONTIN_BINARY_BEHIND;
			}
			else{
				return (msg_fin)?DONE_BINARY:CONTIN_BINARY;
			}
		}
		else if(msg_opcode == 0x9){
			return PING_FRAME;
		}
		else if(msg_opcode == 0xA){
			return PONG_FRAME;
		}
		else{
			return ERROR_FRAME;
		}
	}

	template<class sock_T>
	inline void socket_server<sock_T>::clear_pending(sock_T socket_index){
		std::list<std::string>::iterator it_msg;
		typename std::list<sock_T>::iterator it_socket, it_end;
		std::list<bool>::iterator it_done;
		for(;;){
			it_msg = pending_msg.begin();
			it_socket = pending_socket.begin();
			it_done = pending_done.begin();
			it_end = pending_socket.end();
			for (; it_socket!=it_end; ++it_socket,++it_msg,++it_done) {
				if(*it_socket==socket_index){
					pending_msg.erase(it_msg);
					pending_socket.erase(it_socket);
					pending_done.erase(it_done);
					break;
				}
			}
			if(it_socket==it_end){
				break;
			}
		}
	}
};
