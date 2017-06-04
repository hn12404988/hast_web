#include <iostream>
#include <hast_web/ws_server.hpp>
ws_server server;

auto execute = [&](const short int thread_index){
	std::string file {"inception.jpg"};
	std::vector<char> blob;
	while(server.msg_recv(thread_index)==true){
		std::cout << "msg: " << server.raw_msg[thread_index] << std::endl;
		std::cout << "socketfd: " << server.socketfd[thread_index] << std::endl;
		std::cout << "thread: " << thread_index << std::endl;
		if(server.file_to_blob(file,blob)==false){
			std::cout << "FAIL" << std::endl;
			continue;
		}
		server.echo_back_blob(thread_index,blob);
		std::cout << "SUCCESS" << std::endl;
	}
	server.done(thread_index);
	return;
};

void on_close(const int socket_index){
	std::cout << "CLOSE: " << socket_index << std::endl;
}

bool on_connect(const int socket_index, std::string &user, std::string &password){
	std::cout << "CONNECT: " << socket_index << std::endl;
	std::cout << "User: " << user << std::endl;
	std::cout << "PW: " << password << std::endl;
	return true;
}

bool on_open(const int socket_index, std::string &user, std::string &password){
	std::cout << "OPEN: socket_index" << std::endl;
	std::cout << "User: " << user << std::endl;
	std::cout << "PW: " << password << std::endl;
	return true;
}

int main (int argc, char* argv[]){
	server.execute = execute; 
	server.on_close = on_close;
	server.on_open = on_open;
	server.on_connect = on_connect;
	server.file_max_mb(5);
	if(server.init("8888")==true){
		server.start_accept();
	}
	return 0;
}
