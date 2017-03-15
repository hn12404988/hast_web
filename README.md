# hast_web

Websocket server based on [hast](https://github.com/hn12404988/hast) structure. Let it be a fast server, and can handle requests in parallel. The cost is **Linux-Only**.

## Note that..

* Support part of RFC6455.
* This project is in **alpha** stage. It's still in heavy development.

## Getting Started

* [hast](https://github.com/hn12404988/hast) is running well.
* `openssl-devel` is installed.
* It is **header-only** library. Copy `hast_web` folder to your include path.
* Usage of the library and other details please refer to `example` folder in this moment.
* This project use `std::thread` and `crypto.h`, so compilation command can be:
```
g++ --std=c++11 -pthread -lcrypto .......
```

## How to use

* Please take a look at `examples` folder. This library is pretty easy.
* If you want to know more details about APIs of this library, [wiki page](https://github.com/hn12404988/hast_web/wiki) is where you want to go.
* This library is based on [hast](https://github.com/hn12404988/hast) core. If you want to break down this library to pieces, there is a **Abstraction Layer** [video](https://www.youtube.com/watch?v=EpoL8mSOA6E).

## Framework

* If you are building a linux server, there is another my project called [dalahast](https://github.com/hn12404988/dalahast). It is a example for this project. It contain a complete system from web front-end to hast back-end. 

## Bugs and Issues

This project is still in developed and maintained. Open an issue if you discover some problems.
