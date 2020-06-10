#pragma once

#include <functional>

#include "Channel.h"
#include "Socket.h"

namespace muduo
{
class EventLoop;
class InetAddress;

//传入TCP连接的接收连接服务端
class Acceptor
{
public:
	typedef std::function<void(int sockfd, const InetAddress&)> NewConnectionCallback;

	Acceptor(EventLoop* loop, const InetAddress& listenAddr);

	Acceptor(const Acceptor&) = delete;
	void operator=(const Acceptor&) = delete;


	void setNewConnectionCallback(const NewConnectionCallback& cb)
	{
		newConnectionCallback_ = cb;
	}

	bool listenning() const { return listenning_; }
	void listen();
private:
	void handleRead();

	EventLoop* loop_;			//acceptor所属的eventloop
	Socket	   acceptSocket_;	//接收连接服务端套接字
	Channel	   acceptChannel_;  //接收连接服务端channel渠道
	NewConnectionCallback newConnectionCallback_;	//连接建立事件
	bool	   listenning_;		//监听标识
};

}