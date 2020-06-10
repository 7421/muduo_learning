#pragma once

#include "Callbacks.h"
#include "InetAddress.h"
#include "Buffer.h"

#include <memory>
#include <string>

namespace muduo
{
class Channel;
class EventLoop;
class Socket;

//���ڿͻ��ͷ�������TCP����ʹ�ö˿�

//public std::enable_shared_from_this<TcpConnection>
//����A��share_ptr����������A�ĳ�Ա��������Ҫ�ѵ�ǰ�������Ϊ����������������ʱ��
//����Ҫ����һ��ָ�������share_ptr��

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
	//��һ���Ѿ�������ɵ�sockfd��������TcpConnection
	TcpConnection(EventLoop* loop,
				  const std::string& name,
				  int sockfd,
				  const InetAddress& localAddr,
				  const InetAddress& peerAddr);
	~TcpConnection();

	TcpConnection(const TcpConnection&) = delete;
	void operator=(const TcpConnection&) = delete;

	EventLoop* getLoop() const { return loop_; }
	const std::string& name() const { return name_; }
	const InetAddress& localAddress() { return localAddr_; }
	const InetAddress& peerAddress() { return peerAddr_; }
	bool connected() const { return state_ == kConnected; }
	//Thread safe
	void send(const std::string& message);
	//Thread safe
	void shutdown();
	void setTcpNoDelay(bool on);

	void setConnectionCallback(const ConnectionCallback& cb)
	{  
		connectionCallback_ = cb;
	}

	void setMessageCallback(const MessageCallback& cb)
	{
		messageCallback_ = cb;
	}

	void setWriteCompleteCallback(const WriteCompleteCallback& cb)
	{
		writeCompleteCallback_ = cb;
	}

	void setCloseCallback(const CloseCallback& cb)
	{
		closeCallback_ = cb;
	}
	//��tcpServer����һ���µ�����ʱ ������connectEstablished����
	void connectEstablished();
	//��TcpServer����TcpConnection�Ƴ�ConnectionMap��ʱ����
	void connectDestroyed();
private:
	enum StateE {kConnecting,kConnected ,kDisconnecting,kDisconnected,};
	
	void setState(StateE s) { state_ = s; }
	void handleRead(Timestamp receiveTime);
	void handleWrite();
	void handleClose();
	void handleError();
	
	void sendInLoop(const std::string& message);
	void shutdownInLoop();

	EventLoop* loop_;
	std::string name_;  
	StateE state_;

	//�������ݳ�Ա���û�����
	std::unique_ptr<Socket> socket_;
	std::unique_ptr<Channel> channel_;
	InetAddress localAddr_;	
	InetAddress peerAddr_;

	ConnectionCallback connectionCallback_;
	MessageCallback messageCallback_;
	WriteCompleteCallback writeCompleteCallback_;
	CloseCallback closeCallback_;
	Buffer inputBuffer_;
	Buffer outputBuffer_;
};


}