/**
 * This example mimic a program get base64 string of a image from web front-end, and transfer base64 to binary. Finally write binary into a file.
 * This example show the over-all view about how to receive data partially in hast_web. (Most for large data like file, image...etc)
 * If you have not read another example `wss_server.cpp` yet, it's better head first to that example, then come back.
 * More details about class memebers or methods in wiki page of hast_web repository.
 **/
#include <fstream>
#include <hast_web/wss_server.hpp>
#include <iostream>

wss_server server;

auto execute = [&](const short int index){
	std::string str,name {"test.jpg"};
	std::ofstream ofs;
	int type;
	bool error;
	/**
	 * Each for loop process one request with a complete large data.
	 * This for loop function just like `while(msg_recv()==true){}`.
	 **/
	for(;;){
		/**
		 * Get the first part of data of this request.
		 * Data type of this data return to `type`.
		 * `type` can be:
		 * DONE_TEXT: No more incoming data. This is the first one and also the last one.
		 * CONTIN_TEXT: This is the first data but more data coming.
		 * RECYCLE_THREAD: This thread is ready to be recycled. Break the for loop.
		 **/
		type = server.partially_recv(index);
		if(type==RECYCLE_THREAD){
			break;
		}
		str = "rm -rf "+name;
		system(str.c_str());
		error = false;
		// Open a file, and write data append on the last every time.
		ofs.open (name.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);
		/**
		 * This for loop start to receive all data of this request.
		 * It will process first data, then check whether more data or not.
		 **/
		for(;;){
			//std::cout << "input: " << server.raw_msg[index] << std::endl;
			std::cout << "input size: " << server.raw_msg[index].length() << std::endl;
			ofs.write(server.raw_msg[index].c_str(), server.raw_msg[index].length());
			if(type==DONE_TEXT || type==DONE_BINARY){
				//If this the data is the final one, then break the for loop.
				break;
			}
			/**
			 * `more_recv` only works with `partially_recv`. Try to call `more_recv` under `msg_recv` will cause fatal problems.
			 * Get the next data, and the old one stored in `raw_msg` will be removed.
			 * `type` can be:
			 * DONE_TEXT: This is the last one.
			 * CONTIN_TEXT: More data after this one.
			 * NO_MESSAGE: No data. (This shouldn't be happened after CONTIN_TEXT)
			 * ERROR_FRAME: Error happened. This socket is being closed. Please abandon this request and this socket.
			 **/
			type = server.more_recv(index);
			if(type==NO_MESSAGE || type==ERROR_FRAME){
				error = true;
				break;
			}
		}
		ofs.close();
		std::cout << "thread: " << index << std::endl;
		if(error==true){
			std::cout << "error" << std::endl;
		}
		str = "{\"file_name\":\""+name+"\"}";
		server.echo_back_msg(index,str);
		str = "chown www:www "+name;
		system(str.c_str());
	}
	server.done(index);
	return;
};

int main (int argc, char* argv[]){
	server.execute = execute; 
	if(server.init("/home/tls/workstation/server.crt","/home/tls/workstation/server.key","8888")==true){
		server.start_accept();
	}
	return 0;
}
