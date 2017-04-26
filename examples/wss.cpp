/**
 * This example show the over-all view about how to use hast_web.
 * More details about class memebers or methods in wiki page of hast_web repository.
 **/
#include <iostream>
#include <hast_web/wss_server.hpp>

wss_server server;

/**
 * When a new thread is created, it will start from this lambda function.
 * You can also set this be a normal function.
 **/
auto execute = [&](const short int thread_index){
	//Do something when a thread initiated.
	std::string str;
	while(server.msg_recv(thread_index)==true){
		//This section is how a request be processed.
		//std::cout << "msg: " << server.raw_msg[thread_index] << std::endl;
		//std::cout << "socketfd: " << server.socketfd[thread_index] << std::endl;
		//std::cout << "thread: " << thread_index << std::endl;
		str = "{\"reply\":\"got it\"}";
		std::cout << "Reply: " << str << std::endl;
		server.echo_back_msg(thread_index,str);
	}
	//This thread gonna be recycled. Maybe you need to free() or delete some variables.
	server.done(thread_index);
	return;
};

void on_close(const int socket_index){
	std::cout << "CLOSE: " << socket_index << std::endl;
}

bool on_connect(std::string &user, std::string &password){
	std::cout << "CONNECT" << std::endl;
	std::cout << "User: " << user << std::endl;
	std::cout << "PW: " << password << std::endl;
	return true;
}

bool on_open(SSL *ssl, std::string &user, std::string &password){
	std::cout << "OPEN" << std::endl;
	std::cout << "User: " << user << std::endl;
	std::cout << "PW: " << password << std::endl;
	server.echo_back_msg(ssl,"WELCOME!!");
	return true;
}

int main (int argc, char* argv[]){
	server.execute = execute; //You must assign this member.
	server.on_close = on_close; //Optional
	server.on_open = on_open; //Optional
	server.on_connect = on_connect; //Optional
	/**
	 * Change following tls files' path to yours.
	 * `3` means this program can use 3 threads at most.
	 * `8888` means the port that this program is listening.
	 **/
	if(server.init("/home/tls/server.crt","/home/tls/server.key","8888",3)==true){
		server.start_accept();
	}
	return 0;
}
