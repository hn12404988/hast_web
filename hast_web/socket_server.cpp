socket_server::socket_server(){
	client_addr_size = sizeof(client_addr);
	epollfd = epoll_create1(0);
	ev.events = EPOLLIN;
	ev_tmp.events = EPOLLHUP;
}

socket_server::~socket_server(){
	close(epollfd);
}

bool socket_server::init(hast::tcp_socket::port port, short int unsigned max){
	max_amount = max;
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

void socket_server::done(const short int thread_index){
	if(recv_thread==thread_index){
		recv_thread = -1;
	}
	in_execution[thread_index] = true;
	socketfd[thread_index] = -2;
}

inline void socket_server::recv_epoll(){
	while(got_it==false){}
	for(;;){
		resize();
		waiting_mx.lock();
		for(;;){
			i = epoll_wait(epollfd, events, MAX_EVENTS, 3500);
			if(i>0){
				waiting_mx.unlock();
				break;
			}
		}
		j = i - alive_thread + 1;
		for(;j>0;--j){
			add_thread();
		}
		--i;
		for(;i>=0;--i){
			if(events[i].events!=1){
				close_socket(events[i].data.fd);
				continue;
			}
			get_thread();
			if(j==-1){
				break;
			}
			got_it = false;
			socketfd[j] = events[i].data.fd;
			ev_tmp.data.fd = events[i].data.fd;
			epoll_ctl(epollfd, EPOLL_CTL_MOD, events[i].data.fd,&ev_tmp);
			if(j!=recv_thread){
				while(got_it==false){}
			}
		}
		if(i>=0){
			break;
		}
		else{
			if(socketfd[recv_thread]>=0){
				break;
			}
		}
	}
	recv_thread = -1;
}

bool socket_server::msg_recv(const short int thread_index){
	for(;;){
		raw_msg[thread_index].clear();
		if(socketfd[thread_index]>=0){
			ev.data.fd = socketfd[thread_index];
			epoll_ctl(epollfd, EPOLL_CTL_MOD, socketfd[thread_index],&ev);
			socketfd[thread_index] = -1;
		}
		in_execution[thread_index] = false;
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
			if(socketfd[thread_index]>=0){
				break;
			}
			else if(socketfd[thread_index]==-2){
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
		if(socketfd[thread_index]==-1){
			continue;
		}
		got_it = true;
		int l;
		unsigned char new_char[transport_size];
		std::basic_string<unsigned char> raw_str;
		for(;;){
			l = recv(socketfd[thread_index], new_char, transport_size, 0);
			if(l>0){
				l += raw_str.length();
				raw_str.append(new_char);
				raw_str.resize(l);
				l = 0;
			}
			else{
				break;
			}
		}
		l = raw_str.length()+1;
		unsigned char u_msg[l];
		if(getFrame(&raw_str[0], raw_str.length(), u_msg, l, &l)==TEXT_FRAME){
			raw_msg[thread_index] = reinterpret_cast<char*>(u_msg);
		}
		else{
			close_socket(socketfd[thread_index]);
			continue;
		}
		if(raw_msg[thread_index]==""){
			//client close connection.
			close_socket(socketfd[thread_index]);
			continue;
		}
		in_execution[thread_index] = true;
		return true;
	}
}

inline void socket_server::close_socket(const int socket_index){
	if(on_close!=nullptr){
		on_close(socket_index);
	}
	--alive_socket;
	int a;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, socket_index,nullptr);
	shutdown(socket_index,SHUT_RDWR);
	close(socket_index);
	a = socketfd.size()-1;
	for(;a>=0;--a){
		if(socketfd[a]==socket_index){
			break;
		}
	}
	if(a>=0){
		raw_msg[a].clear();
		socketfd[a] = -1;
	}
}

int socket_server::get_socket(short int thread_index){
	return socketfd[thread_index];
}

void socket_server::start_accept(){
	if(execute==nullptr){
		return;
	}
	int l;
	char new_char[transport_size];
	std::string msg;
	struct pollfd ufds;
	ufds.events = POLLIN;
	int new_socket {1};
	while(new_socket>=0){
		new_socket = accept4(host_socket, (struct sockaddr *)&client_addr, &client_addr_size,SOCK_NONBLOCK);
		if(new_socket>0){
			msg.clear();
			ufds.fd = new_socket;
			if(poll(&ufds,1,3000)<=0 || (ufds.revents & POLLIN)==false){
				shutdown(new_socket,SHUT_RDWR);
				close(new_socket);
				continue;
			}
			for(;;){
				l = recv(new_socket, new_char, transport_size, 0);
				if(l>0){
					l += msg.length();
					msg.append(new_char);
					msg.resize(l);
					l = 0;
				}
				else{
					break;
				}
			}
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
			send(new_socket, msg.c_str(), msg.length(),0);
			if(on_open!=nullptr){
				on_open(new_socket);
			}
			if(recv_thread==-1){
				add_thread();
			}
			++alive_socket;
		}
	}
}

void socket_server::upgrade(std::string &headers){
	std::string key,value;
	size_t msg_len {headers.length()};
	int j,j_max;
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
			j_max = key.length();
			for(j=0;j<j_max;++j){
				if(key[j]==':' && key[j+1]==' '){
					value = key.substr(j+2);
					key = key.substr(0,j);
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
			/*
			if(j==j_max){
				ss["GET"] = key;
			}
			else{
				ss[key] = value;
			}
			*/
			headers = headers.substr(i+1);
			msg_len = headers.length();
			i = 0;
		}
	}
	headers.clear();
}

int socket_server::makeFrameU(WebSocketFrameType frame_type, unsigned char* msg, int msg_length, unsigned char* buffer, int buffer_size)
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

int socket_server::makeFrame(WebSocketFrameType frame_type, const char* msg, int msg_length, char* buffer, int buffer_size)
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

WebSocketFrameType socket_server::getFrame(unsigned char* in_buffer, int in_length, unsigned char* out_buffer, int out_size, int* out_length)
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
		//TODO: if output buffer is too small -- ERROR or resize(free and allocate bigger one) the buffer ?
	}

	memcpy((void*)out_buffer, (void*)(in_buffer+pos), payload_length);
	out_buffer[payload_length] = 0;
	*out_length = payload_length+1;
	
	//printf("TEXT: %s\n", out_buffer);

	if(msg_opcode == 0x0) return (msg_fin)?TEXT_FRAME:INCOMPLETE_TEXT_FRAME; // continuation frame ?
	if(msg_opcode == 0x1) return (msg_fin)?TEXT_FRAME:INCOMPLETE_TEXT_FRAME;
	if(msg_opcode == 0x2) return (msg_fin)?BINARY_FRAME:INCOMPLETE_BINARY_FRAME;
	if(msg_opcode == 0x9) return PING_FRAME;
	if(msg_opcode == 0xA) return PONG_FRAME;

	return ERROR_FRAME;
}
