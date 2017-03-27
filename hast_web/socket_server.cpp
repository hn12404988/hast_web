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

template<>
void socket_server<int>::close_socket(const int socket_index){
	if(socket_index<0){
		return;
	}
	if(on_close!=nullptr){
		on_close(socket_index);
	}
	--alive_socket;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, socket_index,nullptr);
	shutdown(socket_index,SHUT_RDWR);
	close(socket_index);
}

template<>
void socket_server<SSL*>::close_socket(const int socket_index){
	if(socket_index<0){
		return;
	}
	SSL* tmp_ssl;
	tmp_ssl = (*ssl_map)[socket_index];
	if(tmp_ssl!=nullptr){
		SSL_free(tmp_ssl);
	}
	(*ssl_map)[socket_index] = nullptr;
	if(on_close!=nullptr){
		on_close(socket_index);
	}
	--alive_socket;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, socket_index,nullptr);
	shutdown(socket_index,SHUT_RDWR);
	close(socket_index);
}

template<class sock_T>
void socket_server<sock_T>::upgrade(std::string &headers){
	std::string key,value;
	size_t msg_len {headers.length()};
	int tmp,tmp_max;
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
			tmp_max = key.length();
			for(tmp=0;tmp<tmp_max;++tmp){
				if(key[tmp]==':' && key[tmp+1]==' '){
					value = key.substr(tmp+2);
					key = key.substr(0,tmp);
					break;
				}
			}
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
			headers = headers.substr(i+1);
			msg_len = headers.length();
			i = 0;
		}
	}
	headers.clear();
}

template<class sock_T>
bool socket_server<sock_T>::init(hast::tcp_socket::port port, short int unsigned max){
	server_thread<sock_T>::max_amount = max;
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
	server_thread<sock_T>::in_execution[thread_index] = 2;
}

template<>
inline void socket_server<int>::recv_epoll(){
	short int a,b;
	int c;
	while(got_it==false){}
	for(;;){
		resize();
		waiting_mx.lock();
		for(;;){
			a = epoll_wait(epollfd, events, MAX_EVENTS, 3500);
			if(a>0){
				waiting_mx.unlock();
				break;
			}
		}
		b = a - alive_thread + 1;
		for(;b>0;--b){
			add_thread();
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
			in_execution[b] = 1;
			socketfd[b] = c;
			ev_tmp.data.fd = c;
			epoll_ctl(epollfd, EPOLL_CTL_MOD, c,&ev_tmp);
			if(b!=recv_thread){
				while(got_it==false){}
			}
		}
		if(a>=0){
			break;
		}
		else{
			if(b==recv_thread){
				break;
			}
		}
	}
	recv_thread = -1;
}

template<>
inline void socket_server<SSL*>::recv_epoll(){
	short int a,b;
	int c;
	while(got_it==false){}
	for(;;){
		resize();
		waiting_mx.lock();
		for(;;){
			a = epoll_wait(epollfd, events, MAX_EVENTS, 3500);
			if(a>0){
				waiting_mx.unlock();
				break;
			}
		}
		b = a - alive_thread + 1;
		for(;b>0;--b){
			add_thread();
		}
		--a;
		for(;a>=0;--a){
			c = events[a].data.fd;
			if(events[a].events!=1){
				//std::cout << "bad event: " << c << std::endl;
				close_socket(c);
				continue;
			}
			b = get_thread();
			if(b==-1){
				break;
			}
			if((*ssl_map)[c]==nullptr){
				close_socket(c);
				continue;
			}
			else{
				socketfd[b] = (*ssl_map)[c];
			}
			ev_tmp.data.fd = c;
			epoll_ctl(epollfd, EPOLL_CTL_MOD, c,&ev_tmp);
			got_it = false;
			in_execution[b] = 1;
			if(b!=recv_thread){
				while(got_it==false){}
			}
		}
		if(a>=0){
			break;
		}
		else{
			if(b==recv_thread){
				break;
			}
		}
	}
	recv_thread = -1;
}

template<>
bool socket_server<int>::msg_recv(const short int thread_index){
	if(pending[thread_index]!=nullptr){
		if(pending[thread_index]->size()==0){
			delete pending[thread_index];
			pending[thread_index] = nullptr;
		}
		else{
			raw_msg[thread_index] = pending[thread_index]->front();
			pending[thread_index]->pop_front();
			return true;
		}
	}
	int l;
	for(;;){
		l = socketfd[thread_index];
		socketfd[thread_index] = -1;
		if(l>=0){
			ev.data.fd = l;
			epoll_ctl(epollfd, EPOLL_CTL_MOD, l,&ev);
		}
		raw_msg[thread_index].clear();
		in_execution[thread_index] = 0;
		recv_mx.lock();
		if(recv_thread==-1){
			recv_thread = thread_index;
			recv_mx.unlock();
			recv_epoll();
		}
		else{
			recv_mx.unlock();
		}
		for(;;){
			if(in_execution[thread_index]==1){
				break;
			}
			else if(in_execution[thread_index]==2){
				return false;
			}
			else if(recv_thread==-1){
				break;
			}
			else{
				waiting_mx.lock();
				waiting_mx.unlock();
			}
		}
		if(in_execution[thread_index]==0){
			continue;
		}
		got_it = true;
		unsigned char new_char[transport_size];
		std::basic_string<unsigned char> raw_str;
		for(;;){
			l = recv(socketfd[thread_index], new_char, transport_size, MSG_WAITALL);
			if(l>0){
				l += raw_str.length();
				raw_str.append(new_char);
				raw_str.resize(l);
			}
			else{
				break;
			}
		}
		if(l<0 && raw_str.length()==0){
			clear_pending(thread_index);
			close_socket(socketfd[thread_index]);
			continue;
		}
		l = raw_str.length();
		int type;
		unsigned char u_msg[l];
		type = getFrame(&raw_str[0], raw_str.length(), u_msg, l, &l);
		if(type==TEXT_FRAME){
			raw_msg[thread_index] = reinterpret_cast<char*>(u_msg);
		}
		else if(type==CONTIN_TEXT_FRAME){
			raw_msg[thread_index] = reinterpret_cast<char*>(u_msg);
			for(;;){
				raw_str = raw_str.substr(l);
				l = raw_str.length();
				type = getFrame(&raw_str[0], raw_str.length(), u_msg, l, &l);
				if(type==TEXT_FRAME || type==CONTIN_TEXT_FRAME){
					if(pending[thread_index]==nullptr){
						pending[thread_index] = new std::list<std::string>;
					}
					pending[thread_index]->push_back(reinterpret_cast<char*>(u_msg));
					if(type==TEXT_FRAME){
						break;
					}
				}
				else{
					clear_pending(thread_index);
					close_socket(socketfd[thread_index]);
					break;
				}
			}
			if(pending[thread_index]==nullptr){
				continue;
			}
		}
		else{
			close_socket(socketfd[thread_index]);
			continue;
		}
		if(raw_msg[thread_index]==""){
			//client close connection.
			clear_pending(thread_index);
			close_socket(socketfd[thread_index]);
			continue;
		}
		return true;
	}
}

template<>
bool socket_server<SSL*>::msg_recv(const short int thread_index){
	if(pending[thread_index]!=nullptr){
		if(pending[thread_index]->size()==0){
			delete pending[thread_index];
			pending[thread_index] = nullptr;
		}
		else{
			raw_msg[thread_index] = pending[thread_index]->front();
			pending[thread_index]->pop_front();
			return true;
		}
	}
	int l;
	for(;;){
		if(socketfd[thread_index]!=nullptr){
			l = SSL_get_fd(socketfd[thread_index]);
			if(l>=0){
				ev.data.fd = l;
				epoll_ctl(epollfd, EPOLL_CTL_MOD, l,&ev);
			}
			socketfd[thread_index] = nullptr;
		}
		raw_msg[thread_index].clear();
		in_execution[thread_index] = 0;
		recv_mx.lock();
		if(recv_thread==-1){
			//std::cout << "start recv: " << thread_index << std::endl;
			recv_thread = thread_index;
			recv_mx.unlock();
			recv_epoll();
			//std::cout << "end recv: " << thread_index << std::endl;
		}
		else{
			recv_mx.unlock();
		}
		for(;;){
			if(in_execution[thread_index]==1){
				break;
			}
			else if(in_execution[thread_index]==2){
				//std::cout << "recycled: " << thread_index << std::endl;
				return false;
			}
			else if(recv_thread==-1){
				break;
			}
			else{
				waiting_mx.lock();
				waiting_mx.unlock();
			}
		}
		if(in_execution[thread_index]==0){
			continue;
		}
		//std::cout << "got it thread: " << thread_index << std::endl;
		//std::cout << "got it fd: " << SSL_get_fd(socketfd[thread_index]) << std::endl;
		
		got_it = true;
		unsigned char new_char[transport_size];
		std::basic_string<unsigned char> raw_str;
		for(;;){
			l = SSL_read(socketfd[thread_index], new_char, transport_size);
			if(l>0){
				l += raw_str.length();
				raw_str.append(new_char);
				raw_str.resize(l);
			}
			else if(l==0){
				l = SSL_get_error(socketfd[thread_index],l);
				if (l == SSL_ERROR_WANT_READ){
					//std::cout << "Wait for data to be read" << std::endl;
					continue;
				}
				else{
					l = -1;
					break;
				}
				//std::cout << "bad read: " << thread_index << std::endl;
				/*
				l = SSL_get_error(socketfd[thread_index],l);
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
				ERR_print_errors_fp(stderr);
				std::cout << "error queue: " << ERR_get_error()  << std::endl;
				*/
			}
			else{
				break;
			}
		}
		if(l<0 && raw_str.length()==0){
			clear_pending(thread_index);
			close_socket(SSL_get_fd(socketfd[thread_index]));
			continue;
		}
		l = raw_str.length();
		int type;
		unsigned char u_msg[l];
		type = getFrame(&raw_str[0], raw_str.length(), u_msg, l, &l);
		if(type==TEXT_FRAME){
			raw_msg[thread_index] = reinterpret_cast<char*>(u_msg);
		}
		else if(type==CONTIN_TEXT_FRAME){
			raw_msg[thread_index] = reinterpret_cast<char*>(u_msg);
			for(;;){
				raw_str = raw_str.substr(l);
				l = raw_str.length();
				type = getFrame(&raw_str[0], raw_str.length(), u_msg, l, &l);
				if(type==TEXT_FRAME || type==CONTIN_TEXT_FRAME){
					if(pending[thread_index]==nullptr){
						pending[thread_index] = new std::list<std::string>;
					}
					pending[thread_index]->push_back(reinterpret_cast<char*>(u_msg));
					if(type==TEXT_FRAME){
						break;
					}
				}
				else{
					clear_pending(thread_index);
					close_socket(SSL_get_fd(socketfd[thread_index]));
					break;
				}
			}
			if(pending[thread_index]==nullptr){
				continue;
			}
		}
		else{
			//std::cout << "get frame bad: " << thread_index << std::endl;
			close_socket(SSL_get_fd(socketfd[thread_index]));
			continue;
		}
		if(raw_msg[thread_index]==""){
			//std::cout << "raw msg 0: " << thread_index << std::endl;
			//client close connection.
			clear_pending(thread_index);
			close_socket(SSL_get_fd(socketfd[thread_index]));
			continue;
		}
		return true;
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
WebSocketFrameType socket_server<sock_T>::getFrame(unsigned char* in_buffer, int in_length, unsigned char* out_buffer, int out_size, int* out_length)
{
	//printf("getTextFrame()\n");
	if(in_length < 3) return INCOMPLETE_FRAME;

	unsigned char msg_opcode = in_buffer[0] & 0x0F;
	unsigned char msg_fin = (in_buffer[0] >> 7) & 0x01;
	unsigned char msg_masked = (in_buffer[1] >> 7) & 0x01;

	// *** message decoding 

	int payload_length = 0;
	int pos = 2;
	int length_field = in_buffer[1] & (~0x80);
	unsigned int mask = 0;

	//printf("IN:"); for(int i=0; i<20; i++) printf("%02x ",buffer[i]); printf("\n");

	if(length_field <= 125) {
		payload_length = length_field;
	}
	else if(length_field == 126) { //msglen is 16bit!
		payload_length = in_buffer[2] + (in_buffer[3]<<8);
		pos += 2;
	}
	else if(length_field == 127) { //msglen is 64bit!
		payload_length = in_buffer[2] + (in_buffer[3]<<8); 
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
		for(int i=0; i<payload_length; i++) {
			c[i] = c[i] ^ ((unsigned char*)(&mask))[i%4];
		}
	}
	
	if(payload_length > out_size) {
		return OVERSIZE_TEXT_FRAME;
		//TODO: if output buffer is too small -- ERROR or resize(free and allocate bigger one) the buffer ?
	}

	memcpy((void*)out_buffer, (void*)(in_buffer+pos), payload_length);
	out_buffer[payload_length] = 0;
	//*out_length = payload_length+1;
	*out_length = pos+payload_length;
	pos = pos+payload_length;
	
	//printf("TEXT: %s\n", out_buffer);
	if(msg_opcode == 0x0){
		if(in_length>pos){
			return CONTIN_TEXT_FRAME;
		}
		else{
			return (msg_fin)?TEXT_FRAME:INCOMPLETE_TEXT_FRAME;
		}
	}
	else if(msg_opcode == 0x1){
		if(in_length>pos){
			return CONTIN_TEXT_FRAME;
		}
		else{
			return (msg_fin)?TEXT_FRAME:INCOMPLETE_TEXT_FRAME;
		}
	}
	else if(msg_opcode == 0x2){
		if(in_length>pos){
			return CONTIN_BINARY_FRAME;
		}
		else{
			return (msg_fin)?BINARY_FRAME:INCOMPLETE_BINARY_FRAME;
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
void socket_server<sock_T>::clear_pending(short int thread_index){
	if(pending[thread_index]!=nullptr){
		pending[thread_index]->clear();
		delete pending[thread_index];
		pending[thread_index] = nullptr;
	}
}
