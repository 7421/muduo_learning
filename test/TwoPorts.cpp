#include <iostream>
#include <unistd.h>
#include <string>
#include <functional>
#include <thread>

#include "./net/EventLoop.h"
#include "./net/EventLoopThread.h"
#include "./net/SocketsOps.h"
#include "./net/InetAddress.h"
#include "./net/Acceptor.h"
#include "./base/Timestamp.h"

void newConnection1(int sockfd, const muduo::InetAddress& peerAddr)
{
	std::cout << "newConnection:accepted a new connection from " << peerAddr.toHostPort().c_str();
	std::string now = Timestamp::now().toFormattedString();
	write(sockfd, now.c_str(), sizeof(now));
	muduo::sockets::close(sockfd);
}

void newConnection2(int sockfd, const muduo::InetAddress& peerAddr)
{
	std::cout << "newConnection:accepted a new connection from " << peerAddr.toHostPort().c_str();
	std::string now = "hello world!\n";
	write(sockfd, now.c_str(), now.size());
	muduo::sockets::close(sockfd);
}


void threadFun1()
{
	muduo::EventLoop loop;
	muduo::InetAddress listenAddr(5001);
	muduo::Acceptor acceptor(&loop, listenAddr);

	acceptor.setNewConnectionCallback(newConnection1);
	acceptor.listen();
	loop.loop();
}

void threadFun2()
{
	muduo::EventLoop loop;
	muduo::InetAddress listenAddr(1001);
	muduo::Acceptor acceptor(&loop, listenAddr);

	acceptor.setNewConnectionCallback(newConnection2);
	acceptor.listen();
	loop.loop();
}

int main()
{
	std::cout << "main(): pid = " << getpid() << std::endl;
	std::thread thread1(threadFun1);
	std::thread thread2(threadFun2);

	thread1.join();
	thread2.join();
}