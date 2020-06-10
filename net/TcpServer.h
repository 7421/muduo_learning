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
	//�½��ͻ����ӣ�����������TcpConnection�������뵽connections_��
	void newConnection(int sockfd, const InetAddress& peerAddr);
	//Thread safe
	//�ͻ��˶Ͽ����ӣ���Ҫ��mapp��������conn
	void removeConnection(const TcpConnectionPtr& conn); 
	//Not thread safe,but in loop
	void removeConnectionInLoop(const TcpConnectionPtr& conn);

	typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

	EventLoop* loop_;						    //����������EventLoop��
	const std::string name_;					//Tcp������������
	std::unique_ptr<Acceptor> acceptor_;		//Tcp�������˽�����
	std::unique_ptr<EventLoopThreadPool> threadPool_;
	ConnectionCallback        connectionCallback_;  //���ӽ����ص�����
	MessageCallback		      messageCallback_; //��Ϣ����ص�����
	WriteCompleteCallback	  writeCompleteCallback_;
	bool					  started_;			//��ʼ������ʶ
	int						  nextConnId_;		//����number
	ConnectionMap			  connections_;		//������ӳ�� -> Tcp����
};
}