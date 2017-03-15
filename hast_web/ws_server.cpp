inline void ws_server::echo_back_msg(const int socket_index, std::string &msg){
	int len {msg.length()+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME,msg.c_str(),len-1,buf,len);
	send(socket_index, buf, len,0);
}

inline void ws_server::echo_back_msg(const int socket_index, const char* msg){
	int len {strlen(msg)+1};
	char buf[len];
	len = makeFrame(TEXT_FRAME, msg, len-1,buf,len);
	send(socket_index, buf, len,0);
}
