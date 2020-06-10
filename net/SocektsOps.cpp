#include <iostream>
#include <string>

#include "SocketsOps.h"

#include <fcntl.h>
#include <memory.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace muduo;
//���������ռ䣬���Ƶ������������ڵ�ǰ�ļ��У��޷�ͨ����������ļ���ʹ��extern��������������
namespace 
{
typedef struct sockaddr SA;
//sockaddr_in תsockaddr����
const SA* sockaddr_cast(const struct sockaddr_in* addr)
{
	return reinterpret_cast<const SA*>(addr);
}

SA* sockaddr_cast(struct sockaddr_in* addr)
{
	return reinterpret_cast<SA*>(addr);
}

// �ı��׽����ļ�״̬�����ļ�����������
void setNonBlockAndCloseOnExec(int sockfd) 
{
	//�����ļ�״̬��ʶ
	int flags = ::fcntl(sockfd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	int ret = ::fcntl(sockfd, F_SETFL, flags);
	
	//�����ļ���������ʶ
	flags = ::fcntl(sockfd, F_GETFD, 0);
	flags |= FD_CLOEXEC;
	ret = ::fcntl(sockfd, F_SETFD, flags);
}
}
//��������ͨ���׽���
//�ɹ������ļ���������ʧ�ܲ����쳣�������˳�
int sockets::createNonblockingOrDie()
{
	//��������ͨ���׽��֣�IPv4Э���壬�������Ӵ��䷽ʽ��TCPЭ�飬��������exe����������������ʱ�Զ��رո��ļ�������
	int	sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);

	if (sockfd < 0)
	{
		std::cerr << "sockets::createNonblockingOrDie" << std::endl;
		abort();
	}
	return sockfd;
}

int sockets::connect(int sockfd, const struct sockaddr_in& addr)
{
	return ::connect(sockfd, sockaddr_cast(&addr), sizeof addr);
}

//��һ������Э���ַ�����׽���sockfd��ʧ�ܲ����쳣�������˳�
void sockets::bindOrDie(int sockfd, const struct sockaddr_in& addr)
{
	int ret = ::bind(sockfd, sockaddr_cast(&addr), sizeof addr);
	if (sockfd < 0)
	{
		std::cerr << "sockets::bindOrDie" << std::endl;
		abort();
	}
}
//1.���ļ�������תΪ����̬
//2.�涨�ں�Ϊ���׽������õ��Ŷӵ����������
void sockets::listenOrDie(int sockfd)
{
	int ret = ::listen(sockfd, SOMAXCONN);
	if (sockfd < 0)
	{
		std::cerr << "sockets::listenOrDie" << std::endl;
		abort();
	}
}

int sockets::accept(int sockfd, struct sockaddr_in* addr)
{
    socklen_t addrlen = sizeof * addr;

    int connfd = ::accept4(sockfd, sockaddr_cast(addr),
        &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd < 0)
    {
		std::cerr << "Socket::accept " << errno <<std::endl;
		abort();
    }
    return connfd;
}

void sockets::close(int sockfd)
{
	if (::close(sockfd) < 0)
	{
		std::cerr << "sockets::close " <<  std::endl;
		abort();
	}
}

void sockets::shutdownWrite(int sockfd)
{
	if (::shutdown(sockfd, SHUT_WR) < 0)
	{
		std::cerr << "sockets::shutdown Write " << std::endl;
		abort();
	}
}

//����������Э���ַתΪ������Э���ַ
//��buf�ַ�������ʽ���
void sockets::toHostPort(char* buf, size_t size, const struct sockaddr_in& addr)
{
	char host[INET_ADDRSTRLEN] = "INVALID"; //INET_ADDRSTRLEN 16
	::inet_ntop(AF_INET, &addr.sin_addr, host, sizeof host);
	uint16_t port = sockets::networkToHost16(addr.sin_port);
	snprintf(buf, size, "%s:%u", host, port);
}
//����IP��ַ����port�˿ں�,��������Э���ַ
void sockets::fromHostPort(const char* ip, uint16_t port, struct sockaddr_in* addr)
{
	addr->sin_family = AF_INET;
	addr->sin_port = hostToNetwork16(port);
	if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
	{
		std::cerr << "sockets::fromHostPort " << std::endl;
		abort();
	}
}
//���sockfd�׽�������ı���Э���ַ
struct sockaddr_in sockets::getLocalAddr(int sockfd)
{
	struct sockaddr_in localaddr;
	bzero(&localaddr, sizeof localaddr);
	socklen_t addrlen = sizeof(localaddr);
	if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0)
	{
		std::cerr  << "sockets::getLocalAddr";
		abort();
	}
	return localaddr;
}

struct sockaddr_in sockets::getPeerAddr(int sockfd)
{
	struct sockaddr_in peeraddr;
	bzero(&peeraddr, sizeof peeraddr);
	socklen_t addrlen = sizeof(peeraddr);
	if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0)
	{
		std::cerr << "sockets::getPeerAddr";
		abort();
	}
	return peeraddr;
}


int sockets::getSocketError(int sockfd)
{
	int optval;
	socklen_t optlen = sizeof optval;

	if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
	{
		return errno;
	}
	else
	{
		return optval;
	}
}

bool sockets::isSelfConnect(int sockfd)
{
	struct sockaddr_in localaddr = getLocalAddr(sockfd);
	struct sockaddr_in peeraddr = getPeerAddr(sockfd);
	return localaddr.sin_port == peeraddr.sin_port
		&& localaddr.sin_addr.s_addr == peeraddr.sin_addr.s_addr;
}
