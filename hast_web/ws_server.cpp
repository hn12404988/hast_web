void ws_server::start_accept(){
	if(execute==nullptr){
		return;
	}
	int l;
	char new_char[transport_size];
	std::string msg,user,password;
	int new_socket {1};
	while(new_socket>=0){
		new_socket = accept4(host_socket, (struct sockaddr *)&client_addr, &client_addr_size,SOCK_NONBLOCK);
		if(new_socket>0){
			msg.clear();
			if(single_poll(new_socket,3000)==false){
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
			upgrade(msg,user,password);
			if(msg==""){
				shutdown(new_socket,SHUT_RDWR);
				close(new_socket);
				continue;
			}
			if(on_connect!=nullptr){
				if(on_connect(new_socket,user,password)==false){
					shutdown(new_socket,SHUT_RDWR);
					close(new_socket);
					continue;
				}
			}
			send(new_socket, msg.c_str(), msg.length(),0);
			if(on_open!=nullptr){
				if(on_open(new_socket,user,password)==false){
					shutdown(new_socket,SHUT_RDWR);
					close(new_socket);
					continue;
				}
			}
			ev.data.fd = new_socket;
			if(epoll_ctl(epollfd, EPOLL_CTL_ADD, new_socket,&ev)==-1){
				shutdown(new_socket,SHUT_RDWR);
				close(new_socket);
				continue;
			}
			if(recv_thread==-1){
				add_thread();
			}
			++alive_socket;
		}
	}
}

inline void ws_server::echo_back_msg(const short int thread_index, std::string &msg){
	int len {msg.length()+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME,msg.c_str(),len-1,buf,len);
	send_mx.lock();
	send(socketfd[thread_index], buf, len,0);
	send_mx.unlock();
}

inline void ws_server::echo_back_msg(const short int thread_index, const char* msg){
	int len {strlen(msg)+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME, msg, len-1,buf,len);
	send_mx.lock();
	send(socketfd[thread_index], buf, len,0);
	send_mx.unlock();
}

inline void ws_server::echo_back_msg(const int socket_index, std::string &msg){
	if(socket_index<0){
		return;
	}
	int len {msg.length()+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME,msg.c_str(),len-1,buf,len);
	send_mx.lock();
	send(socket_index, buf, len,0);
	send_mx.unlock();
}

inline void ws_server::echo_back_msg(const int socket_index, const char* msg){
	if(socket_index<0){
		return;
	}
	int len {strlen(msg)+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME, msg, len-1,buf,len);
	send_mx.lock();
	send(socket_index, buf, len,0);
	send_mx.unlock();
}
