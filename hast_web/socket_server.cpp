namespace hast_web{
	socket_server::socket_server(){
		reset_addr(hast::tcp_socket::SERVER);
		client_addr_size = sizeof(client_addr);
		epollfd = epoll_create1(0);
		ev.events = EPOLLIN;
		ev_tmp.events = EPOLLHUP;
	}

	socket_server::~socket_server(){
		close(epollfd);
	}

	bool socket_server::single_poll(const int socket_index, const short int time){
		struct pollfd ufds;
		ufds.events = POLLIN;
		ufds.fd = socket_index;
		if(poll(&ufds,1,time)<=0 || (ufds.revents & POLLIN)==false){
			return false;
		}
		return true;
	}

	void socket_server::close_socket(int socket){
		short int a;
		close_mx.lock();
		if(socket<0){
			close_mx.unlock();
			return;
		}
		if(on_close!=nullptr){
			on_close(socket);
		}
		epoll_ctl(epollfd, EPOLL_CTL_DEL, socket,nullptr);
		shutdown(socket,SHUT_RDWR);
		close(socket);
		for(a=0;a<max_thread;++a){
			if(socketfd[a]==socket){
				socketfd[a] = -1;
			}
		}
		clear_pending(socket);
		close_mx.unlock();
	}
	
	void socket_server::close_socket(const short int thread_index){
		close_socket(socketfd[thread_index]);
	}
	

	void socket_server::upgrade(std::string &headers,std::string &user,std::string &password){
		std::string key,value;
		std::size_t msg_len {headers.length()};
		int a,b;
		user.clear();
		password.clear();
		for(std::size_t i=0;i<msg_len;++i){
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

	void socket_server::file_max_mb(std::size_t max){
		file_max = max*1048576;
	}
	
	short int socket_server::get_blob_frame(std::vector<char> &blob, std::size_t size){
		if(size>file_max){
			return 0;
		}
		if(size<=125){
			blob.resize(size+2);
			blob[0] = (char)BINARY_FRAME;
			blob[1] = size;
			return 2;
		}
		else if(size<=65535){
			blob.resize(4+size);
			blob[0] = (char)BINARY_FRAME;
			blob[1] = 126; //16 bit length follows
			blob[2] = (size >> 8) & 0xFF; // leftmost first
			blob[3] = size & 0xFF;
			return 4;
		}
		else{// size>2^16-1
			blob.resize(10+size);
			blob[0] = (char)BINARY_FRAME;
			blob[1] = 127; //64 bit length follows
			blob[2] = 0;
			blob[3] = 0;
			blob[4] = 0;
			blob[5] = 0;
			blob[6] = ((size >> 8*3) & 0xFF);
			blob[7] = ((size >> 8*2) & 0xFF);
			blob[8] = ((size >> 8*1) & 0xFF);
			blob[9] = ((size >> 8*0) & 0xFF);
			return 10;
		}
	}

	bool socket_server::file_to_blob(std::string &file_name,std::vector<char> &blob, short int unsigned extra){
		std::ifstream file(file_name.c_str(),std::ios::binary);
		short int prefix;
		assert(file.is_open());
		if (!file.eof() && !file.fail()){
			file.seekg(0, std::ios_base::end);
			std::streampos fileSize = file.tellg();
			fileSize += extra;
			if(fileSize>file_max){
				return false;
			}
			prefix = get_blob_frame(blob,fileSize);
			fileSize -= extra;
			file.seekg(0, std::ios_base::beg);
			file.read(&blob[prefix], fileSize);
			return true;
		}
		else{
			return false;
		}
	}
	
	bool socket_server::init(hast::tcp_socket::port port, short int unsigned max){
		if(max==0){
			return false;
		}
		max_thread = max;
		server_thread::init();
		if(getaddrinfo(nullptr, port.c_str(), &hints, &res)!=0){
			return false;
		}
		struct addrinfo *p {nullptr};
		int flag {1};
		for(p = res; p != nullptr; p = p->ai_next) {
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
		freeaddrinfo(res);
		res = nullptr;
		if(p==nullptr){
			return false;
		}
		if(listen(host_socket,listen_pending)==0){
			return true;
		}
		else{
			return false;
		}
	}

	void socket_server::done(const short int thread_index){
		/**
		 * This is here for threads break msg_recv loop accidentally.
		 * Threads are only allowed to break msg_recv loop by getting 'false' from it.
		 **/
		status[thread_index] = hast_web::RECYCLE;
	}

	inline void socket_server::recv_epoll(){
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

	bool socket_server::read_loop(const short int thread_index, std::basic_string<unsigned char> &raw_str){
		int a {socketfd[thread_index]};
		int total {0};
		if(a==-1){
			std::cout << "a fail: " << total << std::endl;
			return false;
		}
		if(single_poll(a,3000)==false){
			std::cout << "poll fail: " << total << std::endl;
			return true;
		}
		unsigned char new_char[transport_size];
		int len;
		for(;;){
			len = recv(a, new_char, transport_size,0);
			if(len>0){
				total += len;
				raw_str.append(new_char,len);
			}
			else if(len==0){
				//client close
				std::cout << "read len fail: " << total << std::endl;
				return false;
			}
			else{
				if(errno==EAGAIN || errno==EWOULDBLOCK){
					std::cout << "read len ok: " << total << std::endl;
					return true;
				}
				else{
					std::cout << "read len fail: " << total << std::endl;
					return false;
				}
			}
		}
	}

	WebSocketFrameType socket_server::more_data(const short int thread_index, short int &count){
		std::basic_string<unsigned char> raw_str;
		if(read_loop(thread_index,raw_str)==false){
			close_socket(socketfd[thread_index]);
			std::cout << "more: " << __LINE__ << std::endl;
			return ERROR_FRAME;
		}
		else{
			if(raw_str.length()==0){
				std::cout << "more: " << __LINE__ << std::endl;
				return NO_MESSAGE;
			}
		}
		std::cout << "str len: " << raw_str.length() << std::endl;
		WebSocketFrameType type;
		std::string *str {&raw_msg[thread_index]};
		std::size_t resize_len;
		resize_len = raw_str.length();
		unsigned char u_msg[resize_len];
		type = getFrame(&raw_str[0], resize_len, u_msg, resize_len, &resize_len);
		if(type==DONE_TEXT || type==DONE_TEXT_BEHIND || type==CONTIN_TEXT || type==CONTIN_TEXT_BEHIND || type==DONE_BINARY || type==DONE_BINARY_BEHIND || type==CONTIN_BINARY || type==CONTIN_BINARY_BEHIND){
			str->append(reinterpret_cast<char*>(u_msg), resize_len);
		}
		else{
			close_socket(socketfd[thread_index]);
			std::cout << "more: " << __LINE__ << std::endl;
			return ERROR_FRAME;
		}
		if(type==DONE_TEXT_BEHIND || type==CONTIN_TEXT_BEHIND || type==DONE_BINARY_BEHIND || type==CONTIN_BINARY_BEHIND){
			bool last_is_CONTIN {(type==CONTIN_TEXT_BEHIND || type==CONTIN_BINARY_BEHIND)};
			bool already_done {(type==DONE_TEXT_BEHIND || type==DONE_BINARY_BEHIND)};
			for(;;){
				raw_str = raw_str.substr(resize_len);
				resize_len = raw_str.length();
				type = getFrame(&raw_str[0], resize_len, u_msg, resize_len, &resize_len);
				if(type==DONE_TEXT || type==DONE_TEXT_BEHIND || type==DONE_BINARY || type==DONE_BINARY_BEHIND){
					
					already_done = true;
					if(last_is_CONTIN==true){
						str->append(reinterpret_cast<char*>(u_msg), resize_len);
					}
					else{
						if(type==DONE_TEXT || type==DONE_TEXT_BEHIND){
							str = push_pending(socketfd[thread_index],reinterpret_cast<char*>(u_msg),true,false);
						}
						else{
							str = push_pending(socketfd[thread_index],reinterpret_cast<char*>(u_msg),true,true);
						}
						++count;
					}
					last_is_CONTIN = false;
					if(type==DONE_TEXT || type==DONE_BINARY){
						break;
					}
				}
				else if(type==CONTIN_TEXT || type==CONTIN_TEXT_BEHIND || type==CONTIN_BINARY || type==CONTIN_BINARY_BEHIND){
					if(last_is_CONTIN==true){
						str->append(reinterpret_cast<char*>(u_msg),resize_len);
					}
					else{
						if(type==CONTIN_TEXT || type==CONTIN_TEXT_BEHIND){
							str = push_pending(socketfd[thread_index],reinterpret_cast<char*>(u_msg),false,false);
						}
						else{
							str = push_pending(socketfd[thread_index],reinterpret_cast<char*>(u_msg),false,true);
						}
						++count;
					}
					last_is_CONTIN = true;
					if(type==CONTIN_TEXT || type==CONTIN_BINARY){
						break;
					}
				}
				else{
					close_socket(socketfd[thread_index]);
					std::cout << "more: " << __LINE__ << std::endl;
					return ERROR_FRAME;
				}
			}
			if(already_done==true){
				if(type==DONE_TEXT || type==DONE_BINARY){
					std::cout << "more: " << __LINE__ << std::endl;
					return type;
				}
				else if(type==CONTIN_TEXT){
					std::cout << "more: " << __LINE__ << std::endl;
					return DONE_TEXT_CONTIN;
				}
				else if(type==CONTIN_BINARY){
					std::cout << "more: " << __LINE__ << std::endl;
					return DONE_BINARY_CONTIN;
				}
				else{
					close_socket(socketfd[thread_index]);
					std::cout << "more: " << __LINE__ << std::endl;
					return ERROR_FRAME;
				}
			}
			else{
				if(type==CONTIN_TEXT){
					std::cout << "more: " << __LINE__ << std::endl;
					return CONTIN_TEXT;
				}
				else if(type==CONTIN_BINARY){
					std::cout << "more: " << __LINE__ << std::endl;
					return CONTIN_BINARY;
				}
				else{
					close_socket(socketfd[thread_index]);
					std::cout << "more: " << __LINE__ << std::endl;
					return ERROR_FRAME;
				}
			}
		}
		/**
		 * return type;
		 **/
		else if(type==CONTIN_TEXT || type==CONTIN_BINARY){
			std::cout << "more: " << __LINE__ << std::endl;
			return type;
		}
		else if(type==DONE_TEXT || type==DONE_BINARY){
			std::cout << "more: " << __LINE__ << std::endl;
			std::cout << "more len: " << str->length() << std::endl;
			return type;
		}
	}

	WebSocketFrameType socket_server::more_recv(const short int thread_index){
		raw_msg[thread_index].clear();
		//TODO Remove this useless short int.
		short int useless;
		WebSocketFrameType type;
		type = more_data(thread_index,useless);
		if(type==DONE_TEXT_CONTIN){
			type = DONE_TEXT;
		}
		else if(type==DONE_BINARY_CONTIN){
			type = DONE_BINARY;
		}
		return type;
	}
	
	inline void socket_server::epoll_on(const short int thread_index){
		close_mx.lock();
		if(socketfd[thread_index]>=0){
			ev.data.fd = socketfd[thread_index];
			epoll_ctl(epollfd, EPOLL_CTL_MOD, socketfd[thread_index],&ev);
		}
		close_mx.unlock();
	}

	WebSocketFrameType socket_server::pop_pending(const short int thread_index){
		close_mx.lock();
		if(pending_done.size()==0){
			close_mx.unlock();
			return NO_MESSAGE;
		}
		else{
			bool done,binary;
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
			binary = pending_binary.front();
			pending_binary.pop_front();
			close_mx.unlock();
			if(done==true){
				if(binary==true){
					return DONE_BINARY;
				}
				else{
					return DONE_TEXT;
				}
			}
			else{
				if(binary==true){
					return CONTIN_BINARY;
				}
				else{
					return CONTIN_TEXT;
				}
			}
		}
	}

	std::string* socket_server::push_pending(int socket_index, char *msg, bool done, bool binary){
		std::string *str {nullptr};
		close_mx.lock();
		pending_msg.push_back(msg);
		str = &(pending_msg.back());
		pending_socket.push_back(socket_index);
		pending_done.push_back(done);
		pending_binary.push_back(binary);
		close_mx.unlock();
		return str;
	}

	WebSocketFrameType socket_server::msg_pop_pending(const short int thread_index, short int &count){
		WebSocketFrameType type;
		count = 0;
		type = pop_pending(thread_index);
		if(type!=NO_MESSAGE){
			if(type==CONTIN_TEXT || type==CONTIN_BINARY){
				for(;;){
					type = more_data(thread_index,count);
					if(type==CONTIN_TEXT || type==CONTIN_BINARY){
						continue;
					}
					else{
						break;
					}
				}
				if(type==DONE_TEXT || type==DONE_BINARY){
					epoll_on(thread_index);
					return type;
				}
				else if(type==DONE_TEXT_CONTIN || type==DONE_BINARY_CONTIN){
					return type;
				}
				else{
					close_socket(socketfd[thread_index]);
					return NO_MESSAGE;
				}
			}
			else{
				return type;
			}
		}
		else{
			return NO_MESSAGE;
		}
	}
	
	WebSocketFrameType socket_server::msg_recv(const short int thread_index){
		WebSocketFrameType type;
		short int a,count {0};
		type = msg_pop_pending(thread_index,count);
		if(type!=NO_MESSAGE){
			if(count>0){
				/*
				  for(;count>0;--count){
				  //BUG Thread is probably locked in wait_mx.
				  //TODO Make recv_thread can take job from here.
				  a = get_thread_no_recv();
				  if(a==-1){
				  add_thread();
				  continue;
				  }
				  status[a] = hast_web::READ_PREFIX;
				  }
				*/
			}
			return type;
		}
		for(;;){
			raw_msg[thread_index].clear();
			thread_mx.lock();
			socketfd[thread_index] = -1;
			status[thread_index] = hast_web::WAIT;
			if(recv_thread==-1){
				recv_thread = thread_index;
				thread_mx.unlock();
				recv_epoll();
			}
			else{
				thread_mx.unlock();
			}
			for(;;){
				if(status[thread_index]==hast_web::READ || status[thread_index]==hast_web::READ_PREFIX){
					break;
				}
				else if(status[thread_index]==hast_web::RECYCLE){
					return RECYCLE_THREAD;
				}
				else if(recv_thread==-1){
					if(status[thread_index]==hast_web::WAIT){
						break;
					}
				}
				else{
					wait_mx.lock();
					wait_mx.unlock();
				}
			}
			if(status[thread_index]==hast_web::WAIT){
				continue;
			}
			else if(status[thread_index]==hast_web::READ_PREFIX){
				type = msg_pop_pending(thread_index,count);
				if(type==NO_MESSAGE){
					continue;
				}
				else{
					if(count>0){
						/*
						  for(;count>0;--count){
						  //BUG Thread is probably locked in wait_mx.
						  //TODO Make recv_thread can take job from here.
						  a = get_thread_no_recv();
						  if(a==-1){
						  add_thread();
						  continue;
						  }
						  status[a] = hast_web::READ_PREFIX;
						  }
						*/
						status[thread_index] = hast_web::BUSY;
						return type;
					}
					else{
						status[thread_index] = hast_web::BUSY;
						return type;
					}
				}
			}
			got_it = true;
			count = 0;
			for(;;){
				type = more_data(thread_index,count);
				if(type==CONTIN_TEXT || type==CONTIN_BINARY){
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
					close_socket(socketfd[thread_index]);
					continue;
				}
				else{
					if(raw_msg[thread_index].length()==0){
						close_socket(socketfd[thread_index]);
						continue;
					}
				}
			}
			if(type==DONE_TEXT || type==DONE_BINARY){
				epoll_on(thread_index);
			}
			else{
				if(type==DONE_TEXT_CONTIN){
					type = DONE_TEXT;
				}
				else{//type==DONE_BINARY_CONTIN
					type = DONE_BINARY;
				}
			}
			for(;count>0;--count){
				a = get_thread();
				if(a==-1){
					add_thread();
					continue;
				}
				status[a] = hast_web::READ_PREFIX;
			}
			return type;
			status[thread_index] = hast_web::BUSY;
		}
	}

	WebSocketFrameType socket_server::partially_recv(const short int thread_index){
		WebSocketFrameType type;
		type = pop_pending(thread_index);
		if(type!=NO_MESSAGE){
			return type;
		}
		for(;;){
			raw_msg[thread_index].clear();
			thread_mx.lock();
			epoll_on(thread_index);
			socketfd[thread_index] = -1;
			status[thread_index] = hast_web::WAIT;
			if(recv_thread==-1){
				recv_thread = thread_index;
				thread_mx.unlock();
				recv_epoll();
			}
			else{
				thread_mx.unlock();
			}
			for(;;){
				if(status[thread_index]==hast_web::READ){
					break;
				}
				else if(status[thread_index]==hast_web::RECYCLE){
					return RECYCLE_THREAD;
				}
				else if(recv_thread==-1){
					if(status[thread_index]==hast_web::WAIT){
						break;
					}
				}
				else{
					wait_mx.lock();
					wait_mx.unlock();
				}
			}
			if(status[thread_index]==hast_web::WAIT){
				continue;
			}
			got_it = true;
			//TODO Remove this useless short int.
			short int useless;
			type = more_data(thread_index,useless);
			if(type==ERROR_FRAME){
				close_socket(socketfd[thread_index]);
				continue;
			}
			else if(type==NO_MESSAGE){
				continue;
			}
			else{
				if(raw_msg[thread_index].length()==0){
					close_socket(socketfd[thread_index]);
					continue;
				}
			}
			status[thread_index] = hast_web::BUSY;
			if(type==DONE_TEXT_CONTIN){
				return DONE_TEXT;
			}
			else if(type==DONE_BINARY_CONTIN){
				std::cout << "partial: " << __LINE__ << std::endl;
				return DONE_BINARY;
			}
			else{
				std::cout << "partial: " << __LINE__ << std::endl;
				std::cout << "partial len: " << raw_msg[thread_index].length() << std::endl;
				return type;
			}
		}
	}

	std::size_t socket_server::makeFrameU(WebSocketFrameType frame_type, unsigned char* msg, std::size_t msg_length, unsigned char* buffer, std::size_t buffer_size)
	{
		short int pos = 0;
		std::size_t size = msg_length; 
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

	std::size_t socket_server::makeFrame(WebSocketFrameType frame_type, const char* msg, std::size_t msg_length, char* buffer, std::size_t buffer_size)
	{
		short int pos = 0;
		std::size_t size = msg_length; 
		buffer[pos++] = (char)frame_type; // text frame

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

	WebSocketFrameType socket_server::getFrame(unsigned char* in_buffer, std::size_t in_length, unsigned char* out_buffer, std::size_t out_size, std::size_t* resize_length)
	{
		//printf("getTextFrame()\n");
		if(in_length < 3){
			std::cout << "get: " << __LINE__ << std::endl;
			return ERROR_FRAME;
		}

		unsigned char msg_opcode = in_buffer[0] & 0x0F;
		unsigned char msg_fin = (in_buffer[0] >> 7) & 0x01;
		unsigned char msg_masked = (in_buffer[1] >> 7) & 0x01;

		// *** message decoding 

		std::size_t payload_length = 0;
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
			std::cout << "get: " << __LINE__ << std::endl;
			return INCOMPLETE_FRAME;
		}

		if(msg_masked) {
			mask = *((unsigned int*)(in_buffer+pos));
			//printf("MASK: %08x\n", mask);
			pos += 4;

			// unmask data:
			unsigned char* c = in_buffer+pos;
			for(std::size_t i=0; i<payload_length; i++) {
				c[i] = c[i] ^ ((unsigned char*)(&mask))[i%4];
			}
		}
	
		if(payload_length > out_size) {
			std::cout << "get: " << __LINE__ << std::endl;
			return OVERSIZE_FRAME;
			//It can be text, binary or continual message.
		}

		memcpy((void*)out_buffer, (void*)(in_buffer+pos), payload_length);
		out_buffer[payload_length] = 0;
		*resize_length = payload_length+pos;
		//printf("TEXT: %s\n", out_buffer);
		if(msg_opcode == 0x0){
			if(in_length>*resize_length){
				std::cout << "get: " << __LINE__ << std::endl;
				return (msg_fin)?DONE_TEXT_BEHIND:CONTIN_TEXT_BEHIND;
			}
			else{
				std::cout << "get: " << __LINE__ << std::endl;
				return (msg_fin)?DONE_TEXT:CONTIN_TEXT;
			}
		}
		else if(msg_opcode == 0x1){
			if(in_length>*resize_length){
				std::cout << "get: " << __LINE__ << std::endl;
				return (msg_fin)?DONE_TEXT_BEHIND:CONTIN_TEXT_BEHIND;
			}
			else{
				std::cout << "get: " << __LINE__ << std::endl;
				return (msg_fin)?DONE_TEXT:CONTIN_TEXT;
			}
		}
		else if(msg_opcode == 0x2){
			if(in_length>*resize_length){
				std::cout << "get: " << __LINE__ << std::endl;
				return (msg_fin)?DONE_BINARY_BEHIND:CONTIN_BINARY_BEHIND;
			}
			else{
				std::cout << "get: " << __LINE__ << std::endl;
				std::cout << "resize: " << *resize_length << std::endl;
				return (msg_fin)?DONE_BINARY:CONTIN_BINARY;
			}
		}
		else if(msg_opcode == 0x9){
			std::cout << "get: " << __LINE__ << std::endl;
			return PING_FRAME;
		}
		else if(msg_opcode == 0xA){
			std::cout << "get: " << __LINE__ << std::endl;
			return PONG_FRAME;
		}
		else{
			std::cout << "get: " << __LINE__ << std::endl;
			return ERROR_FRAME;
		}
	}

	inline void socket_server::clear_pending(int socket_index){
		std::list<std::string>::iterator it_msg;
		std::list<int>::iterator it_socket, it_end;
		std::list<bool>::iterator it_done;
		std::list<bool>::iterator it_binary;
		for(;;){
			it_msg = pending_msg.begin();
			it_socket = pending_socket.begin();
			it_done = pending_done.begin();
			it_binary = pending_binary.begin();
			it_end = pending_socket.end();
			for (; it_socket!=it_end; ++it_socket,++it_msg,++it_done) {
				if(*it_socket==socket_index){
					pending_msg.erase(it_msg);
					pending_socket.erase(it_socket);
					pending_done.erase(it_done);
					pending_binary.erase(it_binary);
					break;
				}
			}
			if(it_socket==it_end){
				break;
			}
		}
	}
};
