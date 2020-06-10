#include <iostream>
#include <unistd.h>
#include <string>

#include "./net/EventLoop.h"
#include "./net/SocketsOps.h"
#include "./net/InetAddress.h"
#include "./net/Acceptor.h"
#include "./base/Timestamp.h"

void newConnection(int sockfd, const muduo::InetAddress& peerAddr)
{
	std::cout << "newConnection:accepted a new connection from " << peerAddr.toHostPort().c_str();
	std::string now = Timestamp::now().toFormattedString();
	write(sockfd, now.c_str(), sizeof(now));
	muduo::sockets::close(sockfd);
}

int main()
{
	std::cout << "main(): pid = " << getpid() << std::endl;

	muduo::InetAddress listenAddr(5001);
	muduo::EventLoop   loop;

	muduo::Acceptor acceptor(&loop, listenAddr);
	acceptor.setNewConnectionCallback(newConnection);
	acceptor.listen();
	loop.loop();
}