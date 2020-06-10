#include <iostream>
#include <unistd.h>

#include "./net/EventLoop.h"
#include "./net/SocketsOps.h"
#include "./net/InetAddress.h"
#include "./net/Acceptor.h"

void newConnection(int sockfd, const muduo::InetAddress& peerAddr)
{
	std::cout << "newConnection:accepted a new connection from " << peerAddr.toHostPort().c_str();
	write(sockfd, "How are you?\n", 13);
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