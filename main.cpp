#include <iostream>
#include <thread>
#include <unistd.h>
#include <functional>
#include <fstream>
#include <sstream>
#include "./net/EventLoop.h"
#include "./net/EventLoopThread.h"
#include "./net/TcpServer.h"
#include "./base/AsyncLog.h"
void sendFile(const muduo::TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
        std::cout << "onConnection(): new connection ["
            << conn->name().c_str() << "]" << " from "
            << conn->peerAddress().toHostPort().c_str() << std::endl;
        conn->send("HTTP/1.1 200 ok\r\nconnection: close\r\n\r\n");
        fstream f;
        f.open("HTMLPage.htm");
        stringstream ss;
        ss << f.rdbuf();
        string s;
        s = ss.str();
        conn->send(s);
        conn->shutdown();
    }
}

void onMessage(const muduo::TcpConnectionPtr& conn,
    muduo::Buffer* buf,
    Timestamp receiveTime)
{
    std::cout << "onMessage(): received " << buf->readableBytes()
        << " bytes from connection " << conn->name().c_str()
        << " at " << receiveTime.toFormattedString().c_str()
        << std::endl;
    buf->retrieveAll();
}
int main()
{
    CAsyncLog::init();
    muduo::InetAddress	listenAddr(5001);
    muduo::EventLoop    loop;
    muduo::TcpServer    server(&loop, listenAddr);

    server.setConnectionCallback(sendFile);
    server.setMessageCallback(onMessage);
    server.start();

    loop.loop();
}