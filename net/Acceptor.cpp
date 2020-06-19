#include "Acceptor.h"

#include <functional>

#include "EventLoop.h"
#include "InetAddress.h"
#include "SocketsOps.h"

using namespace muduo;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
	: loop_(loop),
	acceptSocket_(sockets::createNonblockingOrDie()), //�����������˷������׽���Socket
	acceptChannel_(loop, acceptSocket_.fd()),
	listenning_(false)
{   //�˿��ͷź������Ϳ��Ա��ٴ�ʹ��
	acceptSocket_.setReuseAddr(true);
	//��һ������Э���ַ�����������׽���
	acceptSocket_.bindAddress(listenAddr);
	//acceptSocket_.fd()�ɶ�����ʾ�����ӵ���
	//�����ӵ���ʹ��handleRead�ص�
	acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

//listen�������£�
//1. �����ն��ļ���������������̬תΪ����̬
//2. �涨�ں�Ϊ���ļ��������Ŷӵ����������
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
	InetAddress peerAddr(0); //��ſͻ��˵�ַ
	//����������Ӷ��ж�ͷ������һ�����������
	//��accept�ɹ����ں˻��Զ�����һ��ȫ�µ���������
	//�����������ؿͻ���TCP����
	int connfd = acceptSocket_.accept(&peerAddr);
	if (connfd >= 0)
	{
		if (newConnectionCallback_)
			newConnectionCallback_(connfd, peerAddr);
		else
			sockets::close(connfd);
	}
}
