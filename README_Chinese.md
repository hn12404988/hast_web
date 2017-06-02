# hast_web

一個基於[hast](https://github.com/hn12404988/hast)核心的Websocket伺服器，使用C++11編寫，特色為快速而且擁有平行處理的能力，且能接收大型的資料例如檔案，缺點為限定於Linux平台。

## 特色

* 設計得非常容易使用
* 支持大部份RFC6455規章。
* 支持WS和WSS
* 支持**區段接收**，用來接收大型資料（檔案，圖片等等）
* 繼承[hast](https://github.com/hn12404988/hast)：不只是**異步處理**，而是**平行處理**
* 繼承[hast](https://github.com/hn12404988/hast)：動態執行緒數量調整

## 安裝

* `openssl-devel` 套件安裝完畢。
```
yum install openssl-devel
apt-get install libssl-dev
```
* [hast](https://github.com/hn12404988/hast)已安裝
* 把`hast_web`資料夾複製到系統裡的include資料夾
* 這專案使用到`std::thread`, `crypto.h`和`ssl.h`，所以編譯指令可能需包含以下三個套件：
```
# For ws server
g++ --std=c++11 -pthread -lcrypto .......
# For wss server
g++ --std=c++11 -pthread -lcrypto -lssl .......
```

## 如何使用

* 這專案很容易使用，建議直接看`examples`資料夾即可。
* 如果你想要知道更多這專案的細部參數設置，請前往這專案的[wiki](https://github.com/hn12404988/hast_web/wiki)頁面。
* 如果你想知道更多關於[hast](https://github.com/hn12404988/hast)核心，請前往hast的[中文介紹頁面](https://github.com/hn12404988/hast/blob/master/README_Chinese.md)。

## 框架

* 如果你在建立一個Linux伺服器，建議可以參考另一個我的專案叫[dalahast](https://github.com/hn12404988/dalahast)，這是一個完整從Web前端到hast後端的示範框架。
* [hast](https://github.com/hn12404988/hast)是這個專案的核心專案，端口改支援TCP/IP和unix domain，平行處理設計沒有這專案的「單一socket多次接收」的能力，但特別針對[網路拓樸](https://zh.wikipedia.org/wiki/%E7%BD%91%E7%BB%9C%E6%8B%93%E6%89%91)設計額外功能。

## 缺陷和建議

這專案仍在開發中，所以有發現任何缺陷或建議，歡迎使用issue功能回報。
