#include <iostream>
#include <unistd.h>
#include <functional>

#include "./net/EventLoop.h"
#include "./net/InetAddress.h"
#include "./net/TcpServer.h"
#include "./net/Buffer.h"
#include "./net/Connector.h"
#include "./net/TcpClient.h"

#include "./base/Timestamp.h"


std::string message = "Hello\n";

void onConnection(const muduo::TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
        std::cout << "onConnection(): new connection " << conn->name().c_str()
            << " from " << conn->peerAddress().toHostPort().c_str() << std::endl;
        conn->send(message);
    }
    else
    {
        std::cout << "onConnection(): connection " << conn->name().c_str()
            << "is down\n";
    }
}


void onMessage(const muduo::TcpConnectionPtr& conn,
    muduo::Buffer* buf,
    Timestamp receiveTime)
{
    std::cout << "onMessage(): received " << buf->readableBytes()
        << "bytes from connection " << conn->name().c_str()
        << " at " << receiveTime.toFormattedString().c_str() << std::endl;
    std::cout << "onMessage() : [ " << buf->retrieveAsString().c_str() << " ]\n";
}

int main(int argc, char* argv[])
{
    muduo::EventLoop loop;
    muduo::InetAddress serverAddr("127.0.0.1", 5001);
    muduo::TcpClient client(&loop, serverAddr);

    client.setConnectionCallback(onConnection);
    client.setMessageCallback(onMessage);
    client.enableRetry();
    client.connect();
    loop.loop();
}
