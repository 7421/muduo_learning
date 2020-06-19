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
	: loop_(loop),		//Tcpconnection所属eventloop
	  name_(nameArg),	//Tcpconnection对象名字
	  state_(kConnecting),//Tcpconnection当前状态
	  socket_(new Socket(sockfd)),//Tcpconnection的sockfd，服务端/客户端
	  channel_(new Channel(loop,sockfd)), //Tcpconnection channel_渠道
	  localAddr_(localAddr),	//本地协议地址
	  peerAddr_(peerAddr)		//客户端协议地址
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
	写入数据
	1.如果Channel没有监听可写事件且输出缓冲区为空，说明之前没有出现内核缓冲区满的情况，直接写进内核
	2.如果写入内核出错，且错误信息（errno）是EWOULDBLOCK，说明内核缓冲区满，将剩余部分添加到应用层输出缓冲区
	3.如果之前输出缓冲区为空，那么就没有监听内核缓冲区(fd)可写事件，开始监听
*/
void TcpConnection::sendInLoop(const std::string& message)
{
	loop_->assertInLoopThread();
	//写tcp缓冲区的字节数
	ssize_t nwrote = 0;
	// if no thing in output queue, try writing directly
	// 如果输出缓冲区有数据，就不能尝试发送数据了，否则数据会乱，应该直接写到缓冲区中
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
	/* 没出错，且仍有一些数据没有写到tcp缓冲区中，那么就添加到写缓冲区中 */
	if (static_cast<size_t>(nwrote) < message.size())
	{
		outputBuffer_.append(message.data() + nwrote, message.size() - nwrote);
		/* 把没有写完的数据追加到输出缓冲区中，然后开启对可写事件的监听（如果之前没开的话） */
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
	//如果没有正在写入的数据，就关闭
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
	setState(kConnected); //Tcp连接建立完成
	//使能新客户连接的读事件
	channel_->enableReading();
	//返回一个当前类的std::shared_ptr
	//连接完成，调用连接回调函数
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
		handleClose(); //0字符，表示关闭连接
	else
	{
		errno = savedErrno;
		LOGE(" TcpConnection::handleRead ");
		handleError();
	}
		
}
// 当tcp缓冲区可写时调用 
void TcpConnection::handleWrite()
{
	loop_->assertInLoopThread();
	if (channel_->isWriting())
	{
		//试写入写缓冲区的所有数据，返回实际写入的字节数（tcp缓冲区很有可能仍然不能容纳所有数据）
		ssize_t n = ::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());
		if (n > 0)
		{
			//调整写缓冲区的readerIndex
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

