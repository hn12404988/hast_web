# hast_web

一個基於[hast](https://github.com/hn12404988/hast)核心的Websocket伺服器，使用C++11編寫，特色為快速而且擁有平行處理的能力，缺點為限定於Linux平台。

## 留意

* 目前只支持部份RFC6455規章。
* 這專案目前還在非常早期的階段，仍在大量開發。

## 安裝

* [hast](https://github.com/hn12404988/hast)運行正常。
* `openssl-devel` 套件安裝完畢.
* 把`hast_web`資料夾複製到系統裡的include資料夾
* 這專案使用到`std::thread`和`crypto.h`，所以編譯指令可能需包含以下兩個套件：
```
g++ --std=c++11 -pthread -lcrypto .......
```

## 如何使用

* 這專案很容易使用，建議直接看`examples`資料夾即可。
* 如果你想要知道更多這專案的細部參數設置，請前往這專案的[wiki](https://github.com/hn12404988/hast_web/wiki)頁面。
* 如果你想知道更多關於[hast](https://github.com/hn12404988/hast)核心，請前往hast的[抽象層介紹影片](https://www.youtube.com/watch?v=G41F7xHC2bs)。

## 框架

* 如果你在建立一個Linux伺服器，建議可以參考另一個我的專案叫[dalahast](https://github.com/hn12404988/dalahast)，這是一個完整從Web前端到hast後端的示範框架。

## 缺陷和建議

這專案仍在開發中，所以有發現任何缺陷或建議，歡迎使用issue功能回報。

