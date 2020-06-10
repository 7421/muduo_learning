#pragma once

#include <mutex>
#include <memory>

#include "TcpConnection.h"

namespace muduo
{
class Connector;
typedef std::shared_ptr<Connector> ConnectorPtr;

class TcpClient
{
public:
	TcpClient(EventLoop* loop, const InetAddress& serverAddr);
	~TcpClient();

	TcpClient(const TcpClient&) = delete;
	void operator=(const TcpClient&) = delete;

	void connect();
	void disconnect();
	void stop();

	TcpConnectionPtr connection() const
	{
		std::lock_guard<mutex>	lock(mutex_);
		return connection_;
	}

	bool retry() const;
	void enableRetry() { retry_ = true; }

	/// Set connection callback.
	/// Not thread safe.
	void setConnectionCallback(const ConnectionCallback& cb)
	{
		connectionCallback_ = cb;
	}

	/// Set message callback.
	/// Not thread safe.
	void setMessageCallback(const MessageCallback& cb)
	{
		messageCallback_ = cb;
	}

	/// Set write complete callback.
	/// Not thread safe.
	void setWriteCompleteCallback(const WriteCompleteCallback& cb)
	{
		writeCompleteCallback_ = cb;
	}
private:
	//not thread safe, but in loop
	void newConnection(int sockfd);
	//not thread safe, but in loop
	void removeConnection(const TcpConnectionPtr& conn);

	EventLoop* loop_;
	ConnectorPtr  connector_;
	ConnectionCallback connectionCallback_;
	MessageCallback messageCallback_;
	WriteCompleteCallback writeCompleteCallback_;
	bool retry_;   // atmoic
	bool connect_; // atomic
	// always in loop thread
	int nextConnId_;
	mutable mutex mutex_;
	TcpConnectionPtr connection_; // @BuardedBy mutex_
};
}