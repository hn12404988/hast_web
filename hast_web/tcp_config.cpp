inline void tcp_config::reset_addr(bool server_or_client){
	memset(&hints, 0, sizeof(hints));
	if(res!=nullptr){
		freeaddrinfo(res);
		res = nullptr;
	}
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
	if(server_or_client==true){
		hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
	}
}
