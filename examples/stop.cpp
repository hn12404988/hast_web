#include <iostream>
#include <string>

const short int buff_size {300};
std::string root;
short int root_len;

void kill_pid(std::string name){
	FILE *in {nullptr};
	char buff[buff_size];
	int i;
	std::string send,send2;
	bool got_it {false};
	send = "ps axww | grep "+name+".o";
	in = popen(send.c_str(),"r");
	if(in==nullptr){
		std::cout << "Fail on 'ps axww' command" << std::endl;
		return;
	}
	send.clear();
	while(fgets(buff, buff_size, in)!=nullptr){
		send = buff;
		i = 0;
		if(send.find(" sudo ")==std::string::npos && send.find(" grep ")==std::string::npos){
			send2.clear();
			while(send[0]==' '){
				send = send.substr(1);
			}
			while(send[i]!=' '){
				send2 += send[i];
				++i;
			}
			std::cout << "Kill " << name.substr(root_len) << " while pid id: " << send2 << std::endl;
			send2 = "kill -9 "+send2;
			system(send2.c_str());
			got_it = true;
		}
	}
	if(got_it==false){
		std::cout << "Node " << name.substr(root_len) << " is not running." << std::endl;
	}
	pclose(in);
}

void check_pid(std::string name){
	std::string send,send2;
	FILE *in {nullptr};
	int i;
	bool got_it {false};
	char buff[buff_size];
	send = "ps axww | grep "+name+".o";
	in = popen(send.c_str(),"r");
	while(fgets(buff, buff_size, in)!=nullptr){
		send = buff;
		i = 0;
		if(send.find(" sudo ")==std::string::npos && send.find(" grep ")==std::string::npos){
			send2.clear();
			while(send[0]==' '){
				send = send.substr(1);
			}
			while(send[i]!=' '){
				send2 += send[i];
				++i;
			}
			if(got_it==false){
				std::cout << "Node " << name.substr(root_len) << " running with pid: " << send2 << std::endl;
				got_it = true;
			}
			else{
				std::cout << "/&&&&&&&&&&&&  Replicated Nodes &&&&&&&&&&&/" << std::endl;
				std::cout << "Node " << name.substr(root_len) << " running with pid: " << send2 << std::endl;
			}
		}
	}
	if(got_it==false){
		std::cout << "!!!!!!!!!!! Node " << name.substr(root_len) << " is not running !!!!!!!" << std::endl;
	}
	pclose(in);
}

int main(int argc,char* argv[]){
	int i {1};
	std::string str;
	root = "";
	root_len = 0;
	for(;i<argc;++i){
		str = argv[i];
		kill_pid(str);
	}
	return 0;
}
