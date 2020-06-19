#include "TcpServer.h"

#include <functional>
#include <sstream>

#include <stdio.h>
#include <assert.h>

#include "Acceptor.h"
#include "EventLoop.h"
#include "SocketsOps.h"
#include "TcpConnection.h"
#include "EventLoopThreadPool.h"
#include "../base/AsyncLog.h"


using namespace muduo;

TcpServer::TcpServer(EventLoop* loop,const InetAddress& listenAddr)
	:loop_(loop),                         //TcpServer 所属loop    
	 name_(listenAddr.toHostPort()),	  //本地协议监听地址（TcpServer名） 
	 acceptor_(new Acceptor(loop,listenAddr)),//本地连接端
	 threadPool_(new EventLoopThreadPool(loop)),
     started_(false),					  //开始监听标识
	 nextConnId_(1)
{
	acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
}

void TcpServer::setThreadNum(int numThreads)
{
	assert(0 <= numThreads);
	threadPool_->setThreadNum(numThreads);
}



void TcpServer::start()
{
	if (!started_)
	{
		started_ = true;
		threadPool_->start();
	}
	//如果acceptor_没有进入监听状态
	//则通过runInLoop来通知IO线程调用listen函数
	//通过Acceptor接收器进入监听状态
	//使能Acceptro channel的读事件
	if (!acceptor_->listenning())
	{
		loop_->runInLoop(std::bind(&Acceptor::listen,&(*acceptor_)));
	}
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
	loop_->assertInLoopThread();
	char buf[32];
	snprintf(buf, sizeof buf, "#%d", nextConnId_);
	++nextConnId_;
	std::string connName = name_ + buf; //connName : 0.0.0.0:5001#1
	//TcpServer::newConnection [0.0.0.0:5001] - new connection [0.0.0.0:5001#1]
	//from 124.115.222.150:4147
	std::ostringstream os;
	os << "TcpServer::newConnection [" << name_
			  << "] - new connection [" << connName
		      << "] from " << peerAddr.toHostPort() << std::endl;
	LOGI(" %s ", os.str().c_str());
	InetAddress  localAddr(sockets::getLocalAddr(sockfd));

	EventLoop* ioLoop = threadPool_->getNextLoop();
	TcpConnectionPtr conn(
		new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
	connections_[connName] = conn;
	conn->setConnectionCallback(connectionCallback_); //设置连接建立回调函数
	conn->setMessageCallback(messageCallback_);		  //设置消息到达回调函数
	conn->setWriteCompleteCallback(writeCompleteCallback_);
	conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)); //设置连接关闭回调函数
	
																								  //使能channel_读事件
	//回调连接建立函数
	ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
	loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
	loop_->assertInLoopThread();
	LOGI(" TcpServer::removeConnection [ %s ] - coonection %s", name_.c_str(), conn->name().c_str());
	//返回值为1表示删除成功
	size_t n = connections_.erase(conn->name());
	assert(n == 1); (void)n;
	EventLoop* ioLoop = conn->getLoop();
	//通知IO线程，调用连接销毁程序
	ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
