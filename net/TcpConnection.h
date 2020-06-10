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

//用于客户和服务器的TCP连接使用端口

//public std::enable_shared_from_this<TcpConnection>
//当类A被share_ptr管理，且在类A的成员函数里需要把当前类对象作为参数传给其他函数时，
//就需要传递一个指向自身的share_ptr。

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
	//以一个已经连接完成的sockfd，来构造TcpConnection
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
	//当tcpServer接受一个新的连接时 ，调用connectEstablished函数
	void connectEstablished();
	//当TcpServer将此TcpConnection移出ConnectionMap中时调用
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

	//以下数据成员对用户隐藏
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