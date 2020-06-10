#include <iostream>
#include <unistd.h>

#include "./net/EventLoop.h"
#include "./net/InetAddress.h"
#include "./net/TcpServer.h"
#include "./net/Buffer.h"

#include "./base/Timestamp.h"

std::string message1;
std::string message2;

void onConnection(const muduo::TcpConnectionPtr& conn)
{
	if (conn->connected())
	{
		std::cout << "onConnection(): new connection ["
			<< conn->name().c_str() << "]" << " from "
			<< conn->peerAddress().toHostPort().c_str() << std::endl;
		conn->send(message1);
		conn->send(message2);
		conn->shutdown();
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

int main(int argc, char* argv[])
{
	std::cout << getpid() << std::endl;

	int len1 = 100;
	int len2 = 200;


	if (argc > 2)
	{
		len1 = atoi(argv[1]);
		len2 = atoi(argv[2]);
	}

	message1.resize(len1);
	message2.resize(len2);

	std::fill(message1.begin(), message1.end(), 'A');
	std::fill(message2.begin(), message2.end(), 'B');

	muduo::InetAddress listenAddr(5001);
	muduo::EventLoop   loop;

	muduo::TcpServer server(&loop, listenAddr);
	server.setConnectionCallback(onConnection);
	server.setMessageCallback(onMessage);
	server.start();

	loop.loop();
}