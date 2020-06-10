#include "Connector.h"

#include <functional>
#include <iostream>

#include <errno.h>
#include <string.h>

#include "Channel.h"
#include "EventLoop.h"
#include "SocketsOps.h"


using namespace muduo;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
	: loop_(loop),
	  serverAddr_(serverAddr),
	  connect_(false),
	  state_(kDisconnected),
	  retryDelayMs_(kInitRetryDelayMs)
{
	std::cout << "ctor[" << this << "]" << std::endl;
}

Connector::~Connector()
{
	std::cout << "dtor[ " << this << "]" << std::endl;
    loop_->cancel(timerId_);
    assert(!channel_);
}

void Connector::start()
{
	connect_ = true;
	loop_->runInLoop(std::bind(&Connector::startInLoop, this));
}

void Connector::startInLoop()
{
	loop_->assertInLoopThread();
	assert(state_ == kDisconnected);
	//每一次发起连接，重新生成套接字
	if (connect_)
		connect();
	else
		std::cout << "do not connect\n";
}
//每一次发起连接，重新生成套接字
void Connector::connect()
{
	int sockfd = sockets::createNonblockingOrDie();
	int ret = sockets::connect(sockfd, serverAddr_.getSockAddrInet());
	int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno)
    {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
        connecting(sockfd);
        break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
        retry(sockfd);
        break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
        std::cerr << "connect error in Connector::startInLoop " << savedErrno << std::endl;
        sockets::close(sockfd);
        abort();
        break;

    default:
        std::cerr << "Unexpected error in Connector::startInLoop " << savedErrno << std::endl;
        sockets::close(sockfd);
        abort();
        // connectErrorCallback_();
        break;
    }
}

void Connector::restart()
{
    loop_->assertInLoopThread();
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

void Connector::stop()
{
    connect_ = false;
    loop_->cancel(timerId_);
}

void Connector::connecting(int sockfd)
{
    setState(kConnecting);
    assert(!channel_);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback(
        std::bind(&Connector::handleWrite, this)); // FIXME: unsafe
    channel_->setErrorCallback(
        std::bind(&Connector::handleError, this)); // FIXME: unsafe
    // channel_->tie(shared_from_this()); is not working,
    // as channel_ is not managed by shared_ptr
    channel_->enableWriting();
}

int Connector::removeAndResetChannel()
{
    channel_->disableAll();
    loop_->removeChannel(&(*channel_));
    int sockfd = channel_->fd();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    loop_->queueInLoop(std::bind(&Connector::resetChannel, this)); // FIXME: unsafe
    return sockfd;
}

void Connector::resetChannel()
{
    channel_.reset();
}


void Connector::handleWrite()
{
    std::cout << "Connector::handleWrite " << state_ << std::endl;

    if (state_ == kConnecting)
    {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        if (err)
        {
            std::cout << "Connector::handleWrite - SO_ERROR = "
                << err << " " << strerror(err);
            retry(sockfd);
        }
        else if (sockets::isSelfConnect(sockfd))
        {
            std::cout << "Connector::handleWrite - Self connect" << std::endl;
            retry(sockfd);
        }
        else
        {
            setState(kConnected);
            if (connect_)
            {
                newConnectionCallback_(sockfd);
            }
            else
            {
                sockets::close(sockfd);
            }
        }
    }
    else
    {
        // what happened?
        assert(state_ == kDisconnected);
    }
}

void Connector::handleError()
{
    std::cout << "Connector::handleError" << std::endl;
    assert(state_ == kConnecting);

    int sockfd = removeAndResetChannel();
    int err = sockets::getSocketError(sockfd);
    std::cout << "SO_ERROR = " << err << " " << strerror(err) << std::endl;
    retry(sockfd);
}

void Connector::retry(int sockfd)
{
    sockets::close(sockfd);
    setState(kDisconnected);
    if (connect_)
    {
        std::cout << "Connector::retry - Retry connecting to "
            << serverAddr_.toHostPort() << " in "
            << retryDelayMs_ << " milliseconds. " << std::endl;
        timerId_ = loop_->runAfter(retryDelayMs_ / 1000.0,  // FIXME: unsafe
            std::bind(&Connector::startInLoop, this));
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    }
    else
    {
        std::cout << "do not connect" << std::endl;
    }
}
