#include <iostream>
#include <unistd.h>

#include "./net/EventLoop.h"
#include "./net/InetAddress.h"
#include "./net/TcpServer.h"
#include "./net/Buffer.h"

#include "./base/Timestamp.h"

std::string message;

void onConnection(const muduo::TcpConnectionPtr& conn)
{
	if (conn->connected())
	{
		std::cout << "onConnection(): new connection ["
			<< conn->name().c_str() << "]" << " from "
			<< conn->peerAddress().toHostPort().c_str() << std::endl;
		conn->send(message);
	}
	else
	{
		std::cout << " onConnection(): connection " << conn->name()
			<< " is down \n";
	}
}

void onWriteComplete(const muduo::TcpConnectionPtr& conn)
{
	conn->send(message);
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

int main(int argc, char* argv[])
{
	std::cout << getpid() << std::endl;

	std::string line;

	for (int i = 33; i < 127; ++i)
		line.push_back(char(i));

	for (size_t i = 0; i < 127 - 33; ++i)
	{
		message += line.substr(i, 72) + '\n';
	}

	muduo::InetAddress	listenAddr(5001);
	muduo::EventLoop    loop;

	muduo::TcpServer	 server(&loop, listenAddr);
	server.setConnectionCallback(onConnection);
	server.setMessageCallback(onMessage);
	server.setWriteCompleteCallback(onWriteComplete);
	server.start();

	loop.loop();
}