#include "Socket.h"

#include "InetAddress.h"
#include "SocketsOps.h"

#include <memory.h>
#include <netinet/tcp.h>

using namespace muduo;

Socket::~Socket()
{
	sockets::close(sockfd_);
}


void Socket::bindAddress(const InetAddress& addr)
{
	sockets::bindOrDie(sockfd_,addr.getSockAddrInet());
}

void Socket::listen()
{
	sockets::listenOrDie(sockfd_);
}

int Socket::accept(InetAddress* peeraddr)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof addr);
	int connfd = sockets::accept(sockfd_, &addr);
	if (connfd >= 0)
	{
		peeraddr->setSockAddrInet(addr);
	}
	return connfd;
}

void Socket::setReuseAddr(bool on)
{
	int optval = on ? 1 : 0;
	::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Socket::shutdownWrite()
{
	sockets::shutdownWrite(sockfd_);
}

void Socket::setTcpNoDelay(bool on)
{
	int optval = on ? 1 : 0;
	::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
		&optval, sizeof optval);
	// FIXME CHECK
}
