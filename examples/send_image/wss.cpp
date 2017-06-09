#include <iostream>
#include <hast_web/wss_server.hpp>
wss_server server;

auto execute = [&](const short int thread_index){
	std::string file {"inception.jpg"};
	std::vector<char> blob;
	while(server.msg_recv(thread_index)!=RECYCLE_THREAD){
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

int main (int argc, char* argv[]){
	server.execute = execute; 
	server.file_max_mb(5);
	if(server.init("/home/tls/server_1/server.crt","/home/tls/server_1/server.key","8888")==true){
		server.start_accept();
	}
	return 0;
}
