server_thread::~server_thread(){
	i = thread_list.size()-1;
	for(;i>=0;--i){
		if(thread_list[i]!=nullptr){
			if(thread_list[i]->joinable()==true){
				thread_list[i]->join();
				delete thread_list[i];
			}
		}
	}
}

inline void server_thread::resize(){
	//called by recv thread
	i = alive_thread - alive_socket - 1;
	if(i>0){
		j = socketfd.size()-1;
		for(;j>=0;--j){
			if(recv_thread==j){
				continue;
			}
			if(i==0){
				break;
			}
			if(in_execution[j]==false && socketfd[j]==-1 && thread_list[j]!=nullptr && recv_thread!=j){
				socketfd[j] = -2;
			}
			else if(socketfd[j]==-2){
			}
			else{
				continue;
			}
			if(thread_list[j]->joinable()==true){
				thread_list[j]->join();
				delete thread_list[j];
				thread_list[j] = nullptr;
				socketfd[j] = -1;
				--alive_thread;
				--i;
			}
		}
	}
}

inline void server_thread::get_thread(){
	//called by recv thread
	j = socketfd.size()-1;
	for(;j>=0;--j){
		if(recv_thread==j){
			continue;
		}
		if(socketfd[j]==-1 && in_execution[j]==false){
			break;
		}
	}
	if(j==-1){
		if(socketfd[recv_thread]==-1){
			j = recv_thread;
		}
	}
}

inline void server_thread::add_thread(){
	//called by main thread and recv thread
	short int a;
	recv_mx.lock();
	a = socketfd.size();
	if(a>0){
		--a;
		for(;a>=0;--a){
			if(thread_list[a]==nullptr){
				thread_list[a] = new std::thread(execute,a);
				++alive_thread;
				break;
			}
		}
		if(a>=0){
			recv_mx.unlock();
			return;
		}
		else{
			a = socketfd.size();
			if(max_amount>0){
				if(a>=max_amount){
					recv_mx.unlock();
					return;
				}
			}
		}
	}
	in_execution.push_back(true);
	raw_msg.push_back("");
	thread_list.push_back(nullptr);
	thread_list[a] = new std::thread(execute,a);
	++alive_thread;
	socketfd.push_back(-1);
	recv_mx.unlock();
}

