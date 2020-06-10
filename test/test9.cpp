#include <iostream>
#include <unistd.h>

#include "./net/EventLoop.h"
#include "./net/InetAddress.h"
#include "./net/TcpServer.h"
#include "./net/Buffer.h"

#include "./base/Timestamp.h"
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
	muduo::Buffer* buf,
	Timestamp receiveTime)
{
	std::cout << "onMessage(): received " << buf->readableBytes()
		<< " bytes from connection " << conn->name().c_str()
		<< " at " << receiveTime.toFormattedString().c_str()
		<< std::endl;
	conn->send(buf->retrieveAsString());
}

int main()
{
	std::cout << getpid() << std::endl;

	muduo::InetAddress listenAddr(5001);
	muduo::EventLoop   loop;

	muduo::TcpServer server(&loop, listenAddr);
	server.setConnectionCallback(onConnection);
	server.setMessageCallback(onMessage);
	server.start();

	loop.loop();
}