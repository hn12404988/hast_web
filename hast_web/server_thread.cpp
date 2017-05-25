namespace hast_web{
	template<>
	server_thread<int>::server_thread(){}

	template<>
	server_thread<SSL*>::server_thread(){
		ssl_map = new std::map<int,SSL*>;
	}

	template<>
	void server_thread<int>::init(){
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

	template<>
	void server_thread<SSL*>::init(){
		short int a;
		status = new char [max_thread];
		raw_msg = new std::string [max_thread];
		socketfd = new SSL* [max_thread];
		thread_list = new std::thread* [max_thread];
		for(a=0;a<max_thread;++a){
			status[a] = hast_web::BUSY;
			thread_list[a] = nullptr;
			raw_msg[a] = "";
			socketfd[a] = nullptr;
		}
	}

	template<class sock_T>
	void server_thread<sock_T>::destruct(){
		if(thread_list!=nullptr){
			short int a;
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
		if(check_entry!=nullptr){
			delete [] check_entry;
			check_entry = nullptr;
		}
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
		std::map<int,SSL*>::iterator it;
		std::map<int,SSL*>::iterator it_end;
		it = ssl_map->begin();
		it_end = ssl_map->end();
		for(;it!=it_end;++it){
			if(it->second!=nullptr){
				SSL_free(it->second);
				it->second = nullptr;
			}
		}
		delete ssl_map;
		ssl_map = nullptr;
		if(ctx!=nullptr){
			SSL_CTX_free(ctx);
		}
		ERR_free_strings();
		EVP_cleanup();
		destruct();
	}

	template<>
	server_thread<int>::~server_thread(){
		destruct();
	}

	template<class sock_T>
	inline void server_thread<sock_T>::resize(short int amount){
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
				}
				else{
					//TODO Something Wrong
				}
			}
		}
	}

	template<class sock_T>
	short int server_thread<sock_T>::get_thread(){
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

	template<class sock_T>
	short int server_thread<sock_T>::get_thread_no_recv(){
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

	template<class sock_T>
	inline void server_thread<sock_T>::add_thread(){
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
