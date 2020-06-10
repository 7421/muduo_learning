#include <iostream>
#include <unistd.h>

#include "./net/EventLoop.h"
#include "./net/InetAddress.h"
#include "./net/TcpServer.h"
#include "./net/Buffer.h"
#include "./net/Connector.h"
#include "./base/Timestamp.h"

muduo::EventLoop* g_loop;

void connectCallback(int sockfd)
{
    printf("connected.\n");
    g_loop->quit();
}

int main(int argc, char* argv[])
{
    muduo::EventLoop loop;
    g_loop = &loop;
    muduo::InetAddress addr("127.0.0.1", 5001);
    muduo::ConnectorPtr connector(new muduo::Connector(&loop, addr));
    connector->setNewConnectionCallback(connectCallback);
    connector->start();

    loop.loop();
}
