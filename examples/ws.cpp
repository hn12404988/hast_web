#include <iostream>
#include <hast_web/ws_server.h>

ws_server server;

auto execute = [&](const short int index){
	while(server.msg_recv(index)==true){
		std::string str = server.extract_from_raw(index);
		std::cout << str << std::endl;
		str = "I'm here!";
		server.echo_back_msg(server.socketfd[index],str);
	}
	server.done(index);
	return;
};

int main (int argc, char* argv[]){
	server.execute = execute;
	if(server.init("8888",1)==true){
		server.start_accept();
	}
	return 0;
}
