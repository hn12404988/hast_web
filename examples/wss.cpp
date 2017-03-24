/**
 * This example show the over-all view about how to use hast_web.
 * More details about class memebers or methods in wiki page of hast_web repository.
 **/
#include <iostream>
#include <hast_web/wss_server.h>

wss_server server;

/**
 * When a new thread is created, it will start from this lambda function.
 * You can also set this be a normal function.
 **/
auto execute = [&](const short int thread_index){
	//Do something when a thread initiated.
	std::string str;
	int fd;
	while(server.msg_recv(thread_index)==true){
		//This section is how a request be processed.
		fd = server.get_socket_fd(thread_index);
		std::cout << "msg: " << server.raw_msg[thread_index] << std::endl;
		std::cout << "EXECUTE: " << fd << std::endl;
		std::cout << "thread: " << thread_index << std::endl;
		str = "{\"reply\":\"Got it!!\"}";
		server.echo_back_msg(thread_index,str);
	}
	//This thread gonna be recycled. Maybe you need to free() or delete some variables.
	server.done(thread_index);
	return;
};

void on_close(const int socket_index){
	//A socket is closed. Do something here.
	std::cout << "CLOSE: " << socket_index << std::endl;
}

void on_open(SSL* ssl){
	//A socket is opened. Do something here.
	std::cout << "OPEN: " << server.get_socket_fd(ssl) << std::endl;
	server.echo_back_msg(ssl,"Welcome!!");
}

int main (int argc, char* argv[]){
	server.execute = execute; //You must assign this member.
	server.on_close = on_close; //Optional
	server.on_open = on_open; //Optional
	if(server.init("/home/tls/server.crt","/home/tls/server.key","8888",3)==true){
		server.start_accept();
	}
	return 0;
}
