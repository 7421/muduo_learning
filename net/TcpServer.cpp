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
	:loop_(loop),                         //TcpServer ����loop    
	 name_(listenAddr.toHostPort()),	  //����Э�������ַ��TcpServer���� 
	 acceptor_(new Acceptor(loop,listenAddr)),//�������Ӷ�
	 threadPool_(new EventLoopThreadPool(loop)),
     started_(false),					  //��ʼ������ʶ
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
	//���acceptor_û�н������״̬
	//��ͨ��runInLoop��֪ͨIO�̵߳���listen����
	//ͨ��Acceptor�������������״̬
	//ʹ��Acceptro channel�Ķ��¼�
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
	conn->setConnectionCallback(connectionCallback_); //�������ӽ����ص�����
	conn->setMessageCallback(messageCallback_);		  //������Ϣ����ص�����
	conn->setWriteCompleteCallback(writeCompleteCallback_);
	conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)); //�������ӹرջص�����
	
																								  //ʹ��channel_���¼�
	//�ص����ӽ�������
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
	//����ֵΪ1��ʾɾ���ɹ�
	size_t n = connections_.erase(conn->name());
	assert(n == 1); (void)n;
	EventLoop* ioLoop = conn->getLoop();
	//֪ͨIO�̣߳������������ٳ���
	ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
