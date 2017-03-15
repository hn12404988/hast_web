#include <iostream>
#include <hast_web/ws_server.h>

ws_server server;

auto execute = [&](const short int thread_index){
	std::string str;
	while(server.msg_recv(thread_index)==true){
		std::cout << "msg: " << server.raw_msg[thread_index] << std::endl;
		std::cout << "EXECUTE: " << server.socketfd[thread_index] << std::endl;
		str = "Got it!!";
		server.echo_back_msg(server.socketfd[thread_index],str);
	}
	server.done(thread_index);
	return;
};

void on_close(const int socket_index){
	std::cout << "CLOSE: " << socket_index << std::endl;
}

void on_open(const int socket_index){
	std::cout << "OPEN: " << socket_index << std::endl;
	server.echo_back_msg(socket_index,"Welcome!!");
}

int main (int argc, char* argv[]){
	server.execute = execute;
	server.on_close = on_close;
	server.on_open = on_open;
	if(server.init("8888",1)==true){
		server.start_accept();
	}
	return 0;
}
