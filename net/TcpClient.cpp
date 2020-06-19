#include "TcpClient.h"

#include <functional>

#include "Connector.h"
#include "EventLoop.h"
#include "SocketsOps.h"

#include "../base/AsyncLog.h"



namespace muduo
{
	namespace detail
	{
		void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn)
		{
			loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, &(*conn)));
		}
		void removeConnector(const ConnectorPtr& connector)
		{
			//connector->
		}
	}

	TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr)
		: loop_(loop),
		connector_(new Connector(loop, serverAddr)),
		retry_(false),
		connect_(true),
		nextConnId_(1)
	{
		connector_->setNewConnectionCallback(std::bind(&TcpClient::newConnection, this, std::placeholders::_1));
		LOGI( " TcpClient::TcpClien[ %p ] - connector %d", this, connect_);
	}

	TcpClient::~TcpClient()
	{
		LOGI(" TcpClient::~TcpClient[ %p ] - connector %d",this, connector_);
		TcpConnectionPtr conn;
		{
			lock_guard<mutex>	lock(mutex_);
			conn = connection_;
		}
		if (conn)
		{
			CloseCallback cb = std::bind(&detail::removeConnection, loop_, std::placeholders::_1);
			loop_->runInLoop(
				std::bind(&TcpConnection::setCloseCallback, conn, cb));
		}
		else
		{
			connector_->stop();
			loop_->runAfter(1, std::bind(&detail::removeConnector, connector_));
		}
	}

	void TcpClient::connect()
	{
		LOGI("TcpClient::connect[ %p ] - connecting to  %s", this, connector_->serverAddress().toHostPort().c_str());
		connect_ = true;
		connector_->start();
	}

	void TcpClient::disconnect()
	{
		connect_ = false;

		{
			lock_guard<mutex> lock(mutex_);
			if (connection_)
			{
				connection_->shutdown();
			}
		}
	}

	void TcpClient::stop()
	{
		connect_ = false;
		connector_->stop();
	}

	void TcpClient::newConnection(int sockfd)
	{
		loop_->assertInLoopThread();
		InetAddress peerAddr(sockets::getPeerAddr(sockfd));
		char buf[32];
		snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toHostPort().c_str(), nextConnId_);
		++nextConnId_;
		string connName = buf;

		InetAddress localAddr(sockets::getLocalAddr(sockfd));
		// FIXME poll with zero timeout to double confirm the new connection
		// FIXME use make_shared if necessary
		TcpConnectionPtr conn(new TcpConnection(loop_,
			connName,
			sockfd,
			localAddr,
			peerAddr));

		conn->setConnectionCallback(connectionCallback_);
		conn->setMessageCallback(messageCallback_);
		conn->setWriteCompleteCallback(writeCompleteCallback_);
		conn->setCloseCallback(
			std::bind(&TcpClient::removeConnection, this, std::placeholders::_1)); // FIXME: unsafe
		{
			lock_guard<mutex> lock(mutex_);
			connection_ = conn;
		}
		conn->connectEstablished();
	}

	void TcpClient::removeConnection(const TcpConnectionPtr& conn)
	{
		loop_->assertInLoopThread();
		assert(loop_ == conn->getLoop());

		{
			lock_guard<mutex> lock(mutex_);
			assert(connection_ == conn);
			connection_.reset();
		}

		loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
		if (retry_ && connect_)
		{
			LOGI(" TcpClient::connect[ %p ] - Reconnecting to %s", this, connector_->serverAddress().toHostPort().c_str());
			connector_->restart();
		}
	}

}
