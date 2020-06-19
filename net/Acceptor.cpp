#include "Acceptor.h"

#include <functional>

#include "EventLoop.h"
#include "InetAddress.h"
#include "SocketsOps.h"

using namespace muduo;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
	: loop_(loop),
	acceptSocket_(sockets::createNonblockingOrDie()), //创建服务器端非阻塞套接字Socket
	acceptChannel_(loop, acceptSocket_.fd()),
	listenning_(false)
{   //端口释放后立即就可以被再次使用
	acceptSocket_.setReuseAddr(true);
	//将一个本地协议地址赋给服务器套接字
	acceptSocket_.bindAddress(listenAddr);
	//acceptSocket_.fd()可读，表示新连接到来
	//新连接到来使用handleRead回调
	acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

//listen做两件事：
//1. 将接收端文件描述符，从主动态转为被动态
//2. 规定内核为该文件描述符排队的最大连接数
void Acceptor::listen()
{
	loop_->assertInLoopThread();
	listenning_ = true;
	acceptSocket_.listen(); 
	acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
	loop_->assertInLoopThread();
	InetAddress peerAddr(0); //存放客户端地址
	//从已完成连接队列队头返回下一次已完成连接
	//若accept成功，内核会自动生成一个全新的描述符，
	//代表与所返回客户的TCP连接
	int connfd = acceptSocket_.accept(&peerAddr);
	if (connfd >= 0)
	{
		if (newConnectionCallback_)
			newConnectionCallback_(connfd, peerAddr);
		else
			sockets::close(connfd);
	}
}
