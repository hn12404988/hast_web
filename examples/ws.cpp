/**
 * This example show the over-all view about how to use hast_web.
 * More details about class memebers or methods in wiki page of hast_web repository.
 **/
#include <iostream>
#include <hast_web/ws_server.hpp>

ws_server server;

/**
 * When a new thread is created, it will start from this lambda function.
 * You can also set this be a normal function.
 **/
auto execute = [&](const short int thread_index){
	//Do something when a thread initiated.
	std::string str;
	while(server.msg_recv(thread_index)==true){
		//This section is how a request be processed.
		std::cout << "msg: " << server.raw_msg[thread_index] << std::endl;
		std::cout << "socketfd: " << server.socketfd[thread_index] << std::endl;
		std::cout << "thread: " << thread_index << std::endl;
		str = "{\"reply\":\"got it\"}";
		std::cout << "Reply: " << str << std::endl;
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

bool on_connect(const int socket_index, std::string &user, std::string &password){
	//A socket requests to connect. Do something here.
	std::cout << "CONNECT: " << socket_index << std::endl;
	std::cout << "User: " << user << std::endl;
	std::cout << "PW: " << password << std::endl;
	/**
	 * Return true if you want to accept this connection, else return false to close it.
	 * If true, reply to client and establish connection.
	 **/
	return true;
}

bool on_open(const int socket_index, std::string &user, std::string &password){
	//A socket is opened. Do something here.
	std::cout << "OPEN: " << socket_index << std::endl;
	std::cout << "User: " << user << std::endl;
	std::cout << "PW: " << password << std::endl;
	/**
	 * Return true if you want to use this connection, else return false to close it.
	 **/
	server.echo_back_msg(socket_index,"Welcome!!");
	return true;
}

int main (int argc, char* argv[]){
	server.execute = execute; //You must assign this member.
	server.on_close = on_close; //Optional
	server.on_open = on_open; //Optional
	server.on_connect = on_connect; //Optional
	/**
	 * `8888` means the port that this program is listening.
	 * [Optional] `3` means this program can use 3 threads at most. (Default is 2)
	 **/
	if(server.init("8888",3)==true){
		server.start_accept();
	}
	return 0;
}
