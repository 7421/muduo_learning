#pragma once

#include <functional>

#include "Channel.h"
#include "Socket.h"

namespace muduo
{
class EventLoop;
class InetAddress;

//����TCP���ӵĽ������ӷ����
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

	EventLoop* loop_;			//acceptor������eventloop
	Socket	   acceptSocket_;	//�������ӷ�����׽���
	Channel	   acceptChannel_;  //�������ӷ����channel����
	NewConnectionCallback newConnectionCallback_;	//���ӽ����¼�
	bool	   listenning_;		//������ʶ
};

}