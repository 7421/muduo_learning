#include "TcpConnection.h"

#include <functional>
#include <memory>
#include <sstream>

#include <unistd.h>
#include <assert.h>
#include <string.h>

#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"
#include "SocketsOps.h"

#include "../base/AsyncLog.h"
using namespace muduo;

TcpConnection::TcpConnection(EventLoop* loop,
	const std::string& nameArg,
	int sockfd,
	const InetAddress& localAddr,
	const InetAddress& peerAddr)
	: loop_(loop),		//Tcpconnection����eventloop
	  name_(nameArg),	//Tcpconnection��������
	  state_(kConnecting),//Tcpconnection��ǰ״̬
	  socket_(new Socket(sockfd)),//Tcpconnection��sockfd�������/�ͻ���
	  channel_(new Channel(loop,sockfd)), //Tcpconnection channel_����
	  localAddr_(localAddr),	//����Э���ַ
	  peerAddr_(peerAddr)		//�ͻ���Э���ַ
{
	LOGI("TcpConnection::ctor[ %s ] at %p fd = %d",name_.c_str(),this,sockfd);
	channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this,std::placeholders::_1));
	channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
	channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
	channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
}

TcpConnection:: ~TcpConnection()
{
	LOGI("TcpConnection::dtor[ %s ] at %p fd = %d", name_.c_str(), this, channel_->fd());
}

void TcpConnection::send(const std::string& message)
{
	if (state_ == kConnected)
	{
		if (loop_->isInLoopThread())
			sendInLoop(message);
		else
			loop_->runInLoop(std::bind(&TcpConnection::sendInLoop,this,message));
	}
}
/*
	д������
	1.���Channelû�м�����д�¼������������Ϊ�գ�˵��֮ǰû�г����ں˻��������������ֱ��д���ں�
	2.���д���ں˳����Ҵ�����Ϣ��errno����EWOULDBLOCK��˵���ں˻�����������ʣ�ಿ����ӵ�Ӧ�ò����������
	3.���֮ǰ���������Ϊ�գ���ô��û�м����ں˻�����(fd)��д�¼�����ʼ����
*/
void TcpConnection::sendInLoop(const std::string& message)
{
	loop_->assertInLoopThread();
	//дtcp���������ֽ���
	ssize_t nwrote = 0;
	// if no thing in output queue, try writing directly
	// �����������������ݣ��Ͳ��ܳ��Է��������ˣ��������ݻ��ң�Ӧ��ֱ��д����������
	if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
	{
		nwrote = ::write(channel_->fd(), message.data(), message.size());
		if (nwrote >= 0)
		{
			if (static_cast<size_t>(nwrote) < message.size())
				LOGI(" I am going to write more data");
			else if (writeCompleteCallback_) {
				loop_->queueInLoop(
					std::bind(writeCompleteCallback_, shared_from_this()));
			}
		}
		else
		{
			nwrote = 0;
			if (errno != EWOULDBLOCK) {
				LOGE(" TcpConnection::sendInLoop ");
			}
		}
	}
	assert(nwrote >= 0);
	/* û����������һЩ����û��д��tcp�������У���ô����ӵ�д�������� */
	if (static_cast<size_t>(nwrote) < message.size())
	{
		outputBuffer_.append(message.data() + nwrote, message.size() - nwrote);
		/* ��û��д�������׷�ӵ�����������У�Ȼ�����Կ�д�¼��ļ��������֮ǰû���Ļ��� */
		if (!channel_->isWriting())
			channel_->enableWriting();
	}
}

void TcpConnection::shutdown()
{
	if (state_ == kConnected)
	{
		setState(kDisconnecting);
		loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
	}
}


void TcpConnection::shutdownInLoop()
{
	loop_->assertInLoopThread();
	//���û������д������ݣ��͹ر�
	if(!channel_->isWriting())
		socket_->shutdownWrite();
}

void TcpConnection::setTcpNoDelay(bool on)
{
	socket_->setTcpNoDelay(on);
}

void TcpConnection::connectEstablished()
{
	loop_->assertInLoopThread();
	assert(state_ == kConnecting);
	setState(kConnected); //Tcp���ӽ������
	//ʹ���¿ͻ����ӵĶ��¼�
	channel_->enableReading();
	//����һ����ǰ���std::shared_ptr
	//������ɣ��������ӻص�����
	connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed()
{
	loop_->assertInLoopThread();
	assert(state_ == kConnected || state_ == kDisconnecting);
	setState(kDisconnected);
	channel_->disableAll();
	connectionCallback_(shared_from_this());
	loop_->removeChannel(&(*channel_));

}

void TcpConnection::handleRead(Timestamp receiveTime)
{
	int savedErrno = 0;
	ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
	if (n > 0)
		messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
	else if (n == 0)
		handleClose(); //0�ַ�����ʾ�ر�����
	else
	{
		errno = savedErrno;
		LOGE(" TcpConnection::handleRead ");
		handleError();
	}
		
}
// ��tcp��������дʱ���� 
void TcpConnection::handleWrite()
{
	loop_->assertInLoopThread();
	if (channel_->isWriting())
	{
		//��д��д���������������ݣ�����ʵ��д����ֽ�����tcp���������п�����Ȼ���������������ݣ�
		ssize_t n = ::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());
		if (n > 0)
		{
			//����д��������readerIndex
			outputBuffer_.retrieve(n);
			if (outputBuffer_.readableBytes() == 0)
			{
				channel_->disableWriting();
				if (writeCompleteCallback_) {
					loop_->queueInLoop(
						std::bind(writeCompleteCallback_, shared_from_this()));
				}
				if (state_ == kDisconnecting)
					shutdownInLoop();
			}
			else
				LOGI(" I am going to write more data");
		}
		else
		{
			LOGE(" TcpConnection::handleWrite ");
		}
	}
	else
		LOGI(" Connection is down, no more writing ");
}

void TcpConnection::handleClose()
{
	loop_->assertInLoopThread();
	LOGI(" TcpConnection::handleClose state = ");
	assert(state_ == kConnected || state_ == kDisconnecting);
	// we don't close fd, leave it to dtor, so we can find leaks easily.
	channel_->disableAll();
	//must be the last line
	closeCallback_(shared_from_this());
}

__thread char t_errnobuf[512];
void TcpConnection::handleError()
{
	int err = sockets::getSocketError(channel_->fd());
	std::ostringstream os;
	os <<  "TcpConnection::handleError [" << name_
		<< "] - SO_ERROR = " << err <<  " " << strerror(err)<< std::endl;
	LOGE(" %s ", os.str().c_str());
}

