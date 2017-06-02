namespace hast_web{
	server_thread::server_thread(){}
	server_thread::~server_thread(){
		short int a;
		if(thread_list!=nullptr){
			for(;;){
				for(a=0;a<max_thread;++a){
					if(thread_list[a]!=nullptr){
						if(thread_list[a]->joinable()==true){
							thread_list[a]->join();
							delete thread_list[a];
							thread_list[a] = nullptr;
						}
						else{
							//Something Wrong
						}
					}
				}
				for(a=0;a<max_thread;++a){
					if(thread_list[a]!=nullptr){
						break;
					}
				}
				if(a==max_thread){
					break;
				}
			}
			delete [] thread_list;
			thread_list = nullptr;
		}
		if(status!=nullptr){
			delete [] status;
			status = nullptr;
		}
		if(raw_msg!=nullptr){
			delete [] raw_msg;
			raw_msg = nullptr;
		}
		if(socketfd!=nullptr){
			delete [] socketfd;
			socketfd = nullptr;
		}
	}

	void server_thread::init(){
		short int a;
		status = new char [max_thread];
		raw_msg = new std::string [max_thread];
		socketfd = new int [max_thread];
		thread_list = new std::thread* [max_thread];
		for(a=0;a<max_thread;++a){
			status[a] = hast_web::BUSY;
			thread_list[a] = nullptr;
			raw_msg[a] = "";
			socketfd[a] = -1;
		}
	}

	inline void server_thread::resize(short int amount){
		short int a {0};
		for(;a<max_thread;++a){
			if(recv_thread==a){
				continue;
			}
			if(status[a]==hast_web::WAIT && thread_list[a]!=nullptr){
				if(amount>0){
					status[a] = hast_web::RECYCLE;
					--amount;
					continue;
				}
			}
			else if(status[a]==hast_web::RECYCLE){
				if(thread_list[a]->joinable()==true){
					thread_list[a]->join();
					delete thread_list[a];
					thread_list[a] = nullptr;
					status[a] = hast_web::BUSY;
					socketfd[a] = -1;
				}
				else{
					//TODO Something Wrong
				}
			}
		}
	}

	short int server_thread::get_thread(){
		thread_mx.lock();
		short int a {0};
		if(recv_thread==-1){
			for(;a<max_thread;++a){
				if(status[a]==hast_web::WAIT){
					break;
				}
			}
			if(a<max_thread){
				status[a] = hast_web::GET;
			}
			else{
				a = -1;
			}
		}
		else{
			for(;a<max_thread;++a){
				if(recv_thread==a){
					continue;
				}
				if(status[a]==hast_web::WAIT){
					break;
				}
			}
			if(a==max_thread){
				if(status[recv_thread]==hast_web::WAIT){
					a = recv_thread;
					status[a] = hast_web::GET;
				}
				else{
					a = -1;
				}
			}
			else{
				status[a] = hast_web::GET;
			}
		}
		thread_mx.unlock();
		return a;
	}

	short int server_thread::get_thread_no_recv(){
		thread_mx.lock();
		short int a {0};
		for(;a<max_thread;++a){
			if(recv_thread==a){
				continue;
			}
			if(status[a]==hast_web::WAIT){
				break;
			}
		}
		if(a==max_thread){
			status[a] = hast_web::GET;
		}
		else{
			a = -1;
		}
		thread_mx.unlock();
		return a;
	}

	inline void server_thread::add_thread(){
		short int a {0};
		thread_mx.lock();
		for(;a<max_thread;++a){
			if(thread_list[a]==nullptr){
				thread_list[a] = new std::thread(execute,a);
				break;
			}
		}
		thread_mx.unlock();
	}
};
