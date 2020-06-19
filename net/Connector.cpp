#include "Connector.h"

#include <functional>


#include <errno.h>
#include <string.h>

#include "../base/AsyncLog.h"
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
	LOGI( "ctor[ %p ]" , this);
}

Connector::~Connector()
{
    LOGI("dtor[ %p ]", this);
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
        LOGI("do not connect");
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
        sockets::close(sockfd);
        LOGE(" connect error in Connector::startInLoop %d ", savedErrno);
        break;

    default:
        sockets::close(sockfd);
        LOGE(" Unexpected error in Connector::startInLoop  %d ", savedErrno);
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
    LOGI(" Connector::handleWrite %d " ,state_ );

    if (state_ == kConnecting)
    {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        if (err)
        {
            LOGI("Connector::handleWrite - SO_ERROR = %d %s", err, strerror(err));
            retry(sockfd);
        }
        else if (sockets::isSelfConnect(sockfd))
        {
            LOGI("Connector::handleWrite - Self connect");
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
    LOGI("Connector::handleError");
    assert(state_ == kConnecting);

    int sockfd = removeAndResetChannel();
    int err = sockets::getSocketError(sockfd);
    LOGI("SO_ERROR = %d %s", err, strerror(err));
    retry(sockfd);
}

void Connector::retry(int sockfd)
{
    sockets::close(sockfd);
    setState(kDisconnected);
    if (connect_)
    {
        LOGI("Connector::retry - Retry connecting to %s in %d  milliseconds. ", serverAddr_.toHostPort().c_str(), retryDelayMs_);
        timerId_ = loop_->runAfter(retryDelayMs_ / 1000.0,  // FIXME: unsafe
            std::bind(&Connector::startInLoop, this));
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    }
    else
    {
        LOGI("do not connect");
    }
}
