#include "InetAddress.h"

#include "SocketsOps.h"

#include <string>
#include <netinet/in.h>
#include <memory.h>

/*
	struct sockaddr_in{
		sa_family_t		sin_family;
		uint16_t		sin_port;
		struct in_addr  sin_addr;
	};

  //Internet address
  typedef uint32_t in_addr_t;
  struct in_addr{
		in_addr_t	s_addr;
	};
*/
using namespace muduo;

static const in_addr_t kInaddrAny = INADDR_ANY;

//根据端口号创建网络协议地址
InetAddress::InetAddress(uint16_t port)
{
	memset(&addr_,0, sizeof addr_); //addr_结构体初始化为0
	addr_.sin_family = AF_INET;
	addr_.sin_addr.s_addr = sockets::hostToNetwork32(kInaddrAny);
	addr_.sin_port = sockets::hostToNetwork16(port);
}
//根据IP地址和端口号创建网络协议地址
InetAddress::InetAddress(const std::string& ip, uint16_t port)
{
	memset(&addr_, 0, sizeof addr_);
	sockets::fromHostPort(ip.c_str(), port, &addr_);
}


std::string InetAddress::toHostPort() const
{
	char buf[32];
	sockets::toHostPort(buf, sizeof buf, addr_);
	return buf;
}
