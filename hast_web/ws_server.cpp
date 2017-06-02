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
					msg.append(new_char,l);
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
			if(write(new_socket,msg)==false){
				shutdown(new_socket,SHUT_RDWR);
				close(new_socket);
				continue;
			}
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
		}
	}
}

inline void ws_server::echo_back_msg(const short int thread_index, std::string &msg){
	int len {msg.length()+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME,msg.c_str(),len-1,buf,len);
	buf[len] = '\0';
	msg = buf;
	write(socketfd[thread_index],msg);
}

inline void ws_server::echo_back_msg(const short int thread_index, const char* msg){
	int len {strlen(msg)+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME, msg, len-1,buf,len);
	buf[len] = '\0';
	std::string tmp_str(buf);
	write(socketfd[thread_index],tmp_str);
}

inline void ws_server::echo_back_msg(const int socket_index, std::string &msg){
	if(socket_index<0){
		return;
	}
	int len {msg.length()+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME,msg.c_str(),len-1,buf,len);
	buf[len] = '\0';
	msg = buf;
	write(socket_index,msg);
}

inline void ws_server::echo_back_msg(const int socket_index, const char* msg){
	if(socket_index<0){
		return;
	}
	int len {strlen(msg)+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME, msg, len-1,buf,len);
	buf[len] = '\0';
	std::string tmp_msg(buf);
	write(socket_index,tmp_msg);
}

inline bool ws_server::write(int socket_index, std::string &msg){
	int flag,len {msg.length()};
	const char* i {msg.c_str()};
	for(;;){
		flag = send(socket_index , i , len , 0);
		if(flag==-1){
			if(errno==EAGAIN || errno==EWOULDBLOCK){
				continue;
			}
			else{
				close_socket(socket_index);
				return false;
			}
		}
		else{
			if(flag==len){
				break;
			}
			else{
				msg = msg.substr(flag);
				i = msg.c_str();
				len = msg.length();
			}
		}
	}
	return true;
}

