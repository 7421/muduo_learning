#pragma once

#include "Callbacks.h"
#include "TcpConnection.h"

#include <map>
#include <string>
#include <memory>

namespace muduo
{
class Acceptor;
class EventLoop;
class EventLoopThreadPool;

class TcpServer
{
public:
	TcpServer(EventLoop* loop, const InetAddress& listenAddr);
	~TcpServer();

	TcpServer(const TcpServer&) = delete;
	void operator=(const TcpServer&) = delete;

	/// Set the number of threads for handling input.
	///
	/// Always accepts new connection in loop's thread.
	/// Must be called before @c start
	/// @param numThreads
	/// - 0 means all I/O in loop's thread, no thread will created.
	///   this is the default value.
	/// - 1 means all I/O in another thread.
	/// - N means a thread pool with N threads, new connections
	///   are assigned on a round-robin basis.
	void setThreadNum(int numThreads);

	// Starts the server if it's not listenning.
	//It's harmless to call it multiple times.
	//Thread safe
	void start();

	// Set connection callback.
	// Not thread safe.
	void setConnectionCallback(const ConnectionCallback& cb)
	{
		connectionCallback_ = cb;
	}
	//set message callback
	//Not thread safe
	void setMessageCallback(const MessageCallback& cb)
	{
		messageCallback_ = cb;
	}

	void setWriteCompleteCallback(const WriteCompleteCallback& cb)
	{
		writeCompleteCallback_ = cb;
	}
private:
	//Not thread safe, but in loop
	//新建客户连接，创建新连接TcpConnection，并加入到connections_中
	void newConnection(int sockfd, const InetAddress& peerAddr);
	//Thread safe
	//客户端断开连接，需要从mapp表中移走conn
	void removeConnection(const TcpConnectionPtr& conn); 
	//Not thread safe,but in loop
	void removeConnectionInLoop(const TcpConnectionPtr& conn);

	typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

	EventLoop* loop_;						    //接收器所处EventLoop环
	const std::string name_;					//Tcp服务器端名字
	std::unique_ptr<Acceptor> acceptor_;		//Tcp服务器端接收器
	std::unique_ptr<EventLoopThreadPool> threadPool_;
	ConnectionCallback        connectionCallback_;  //连接建立回调函数
	MessageCallback		      messageCallback_; //消息到达回调函数
	WriteCompleteCallback	  writeCompleteCallback_;
	bool					  started_;			//开始监听标识
	int						  nextConnId_;		//连接number
	ConnectionMap			  connections_;		//连接名映射 -> Tcp连接
};
}