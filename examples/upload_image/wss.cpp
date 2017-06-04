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

// Just a function to transfer base64 to binary.
static const std::string base64_chars = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

// Just a function to transfer base64 to binary.
static inline bool is_base64(unsigned char c) {
	return (isalnum(c) || (c == '+') || (c == '/'));
}

// Just a function to transfer base64 to binary.
void base64_decode(const std::string &encoded_string, std::string &img_code) {
	int in_len = encoded_string.length();
	int i = 0;
	int j = 0;
	int in_ = 0;
	img_code.clear();
	unsigned char char_array_4[4], char_array_3[3];
	while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i ==4) {
			for (i = 0; i <4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				img_code += char_array_3[i];
			i = 0;
		}
	}

	if (i) {
		for (j = i; j <4; j++)
			char_array_4[j] = 0;

		for (j = 0; j <4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++) img_code += char_array_3[j];
	}
}

auto execute = [&](const short int index){
	std::string img_code;
	std::string name;
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
		 * Done_TEXT: No more incoming data. This is the first one and also the last one.
		 * CONTIN_TEXT: This is the first data but more data coming.
		 * RECYCLE_THREAD: This thread is ready to be recycled. Break the for loop.
		 **/
		type = server.partially_recv(index);
		if(type==RECYCLE_THREAD){
			break;
		}
		name = "test.jpg";
		error = false;
		// Open a file, and write data append on the last every time.
		ofs.open (name.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);
		/**
		 * This for loop start to receive all data of this request.
		 * It will process first data, then check whether more data or not.
		 **/
		for(;;){
			img_code.clear();
			std::cout << "input size: " << server.raw_msg[index].length() << std::endl;
			base64_decode(server.raw_msg[index],img_code);
			std::cout << "img_code size: " << img_code.length() << std::endl;
			ofs.write(img_code.c_str(), img_code.length());
			if(type==DONE_TEXT){
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
		name = "{\"file_name\":\""+name+"\"}";
		server.echo_back_msg(index,name);
	}
	server.done(index);
	return;
};

void on_close(const int socket_index){
	std::cout << "CLOSE: " << socket_index << std::endl;
}

bool on_open(SSL* ssl, std::string &user, std::string &password){
	std::cout << "OPEN: " << SSL_get_fd(ssl) << std::endl;
	std::cout << "USER: " << user << std::endl;
	std::cout << "PASSWORD: " << password << std::endl;
	server.echo_back_msg(ssl,"Welcome!!");
	return true;
}

int main (int argc, char* argv[]){
	server.execute = execute; 
	server.on_close = on_close;
	server.on_open = on_open;
	if(server.init("/home/tls/server_2/server.crt","/home/tls/server_2/server.key","8888")==true){
		server.start_accept();
	}
	return 0;
}
