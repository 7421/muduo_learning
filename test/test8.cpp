#include <iostream>
#include <unistd.h>

#include "./net/EventLoop.h"
#include "./net/InetAddress.h"
#include "./net/TcpServer.h"

void onConnection(const muduo::TcpConnectionPtr& conn)
{
	if (conn->connected())
	{
		std::cout << "onConnection(): new connection ["
			<< conn->name().c_str() << "]" << " from "
			<< conn->peerAddress().toHostPort().c_str() << std::endl;
	}
	else
	{
		std::cout << " onConnection(): connection " << conn->name()
			<< " is down \n";
	}
}

void onMessage(const muduo::TcpConnectionPtr& conn,
	const char* data,
	ssize_t len)
{
	std::cout << "onMessage(): received " << len
		<< " bytes from connection " << conn->name().c_str()
		<< std::endl;
}

int main()
{
	std::cout << getpid();

	muduo::InetAddress listenAddr(5001);
	muduo::EventLoop   loop;

	muduo::TcpServer server(&loop, listenAddr);
	server.setConnectionCallback(onConnection);
	server.setMessageCallback(onMessage);
	server.start();

	loop.loop();
}