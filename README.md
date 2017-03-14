# hast_web

Websocket server based on [hast](https://github.com/hn12404988/hast) structure. Let it be a fast server, and can handle requests in parallel. The cost is **Linux-Only**.

## Note that..

* Support part of RFC6455.
* This project is in **alpha** stage. It's still in heavy development.

## Getting Started

* [hast](https://github.com/hn12404988/hast) is running well.
* It's **header-only** library. Copy `hast_web` folder to your include path.
* Usage of the library and other details please refer to `example` folder in this moment.
* This project use `std::thread` and `crypto.h`, so compilation command can be:
```
g++ --std=c++11 -pthread -lcrypto .......
```

## More Information

* Please go to [hast](https://github.com/hn12404988/hast) page in this moment. I'm still working on this project.

## Framework

* If you are building a linux server, there is another my project called [dalahast](https://github.com/hn12404988/dalahast). It is a example for this project. It contain a complete system from web front-end to hast back-end. 

## Bugs and Issues

This project is still in developed and maintained. Open an issue if you discover some problems.
