namespace hast_web{
	template<>
	server_thread<int>::server_thread(){}

	template<>
	server_thread<SSL*>::server_thread(){
		ssl_map = new std::map<int,SSL*>;
	}
	
	template<>
	bool server_thread<SSL*>::wss_init(const char* crt, const char* key){
		SSL_load_error_strings();	
		OpenSSL_add_ssl_algorithms();
		SSL_library_init();
		const SSL_METHOD *method;
		method = SSLv23_server_method();
		ctx = SSL_CTX_new(method);
		if (!ctx) {
			perror("Unable to create SSL context");
			ERR_print_errors_fp(stderr);
			return false;
		}
		//SSL_CTX_set_ecdh_auto(ctx, 1); This is removed in newer openssl.
		/* Set the key and cert */
		if (SSL_CTX_use_certificate_file(ctx, crt, SSL_FILETYPE_PEM) < 0) {
			ERR_print_errors_fp(stderr);
			return false;
		}
		if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) < 0 ) {
			ERR_print_errors_fp(stderr);
			return false;
		}
		return true;
	}

	template<>
	server_thread<SSL*>::~server_thread(){
		short int a;
		std::map<int,SSL*>::iterator it;
		std::map<int,SSL*>::iterator it_end;
		a = thread_list.size()-1;
		for(;a>=0;--a){
			if(thread_list[a]!=nullptr){
				if(thread_list[a]->joinable()==true){
					thread_list[a]->join();
					delete thread_list[a];
				}
			}
		}
		for(;it!=it_end;++it){
			if(it->second!=nullptr){
				SSL_free(it->second);
			}
		}
		delete ssl_map;
		if(ctx!=nullptr){
			SSL_CTX_free(ctx);
		}
		ERR_free_strings();
		EVP_cleanup();
	}

	template<>
	server_thread<int>::~server_thread(){
		short int a;
		a = thread_list.size()-1;
		for(;a>=0;--a){
			if(thread_list[a]!=nullptr){
				if(thread_list[a]->joinable()==true){
					thread_list[a]->join();
					delete thread_list[a];
				}
			}
		}
	}

	template<class sock_T>
	inline void server_thread<sock_T>::resize(){
		short int a,b;
		a = alive_thread - alive_socket - 1;
		if(a>0){
			b = socketfd.size()-1;
			for(;b>=0;--b){
				if(recv_thread==b){
					continue;
				}
				if(a==0){
					break;
				}
				if(in_execution[b]==0 && thread_list[b]!=nullptr){
					in_execution[b] = 2;
				}
				else if(in_execution[b]==2){
				}
				else{
					continue;
				}
				if(thread_list[b]->joinable()==true){
					thread_list[b]->join();
					delete thread_list[b];
					thread_list[b] = nullptr;
					--alive_thread;
					--a;
				}
			}
		}
	}

	template<class sock_T>
	short int server_thread<sock_T>::get_thread(){
		short int a;
		a = socketfd.size()-1;
		for(;a>=0;--a){
			if(recv_thread==a){
				continue;
			}
			if(in_execution[a]==0){
				break;
			}
		}
		if(a==-1){
			a = recv_thread;
		}
		return a;
	}

	template<>
	inline void server_thread<int>::add_thread(){
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
		socketfd.push_back(-1);
		in_execution.push_back(1);
		raw_msg.push_back("");
		thread_list.push_back(nullptr);
		pending_done.push_back(true);
		thread_list[a] = new std::thread(execute,a);
		++alive_thread;
		recv_mx.unlock();
	}

	template<>
	inline void server_thread<SSL*>::add_thread(){
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
		socketfd.push_back(nullptr);
		in_execution.push_back(1);
		raw_msg.push_back("");
		thread_list.push_back(nullptr);
		pending_done.push_back(true);
		thread_list[a] = new std::thread(execute,a);
		++alive_thread;
		recv_mx.unlock();
	}
};
