#pragma once

#include <string>
#include <netinet/in.h>

namespace muduo
{
	class InetAddress
	{
	public:
		//通过给定端口，构造目的端地址
		explicit InetAddress(uint16_t port);

		//通过给定的IP地址和端口号构造 目的端地址
		//ip 格式 "1.2.3.4"
		InetAddress(const std::string& ip, uint16_t port);

		InetAddress(const struct sockaddr_in& addr)
			: addr_(addr)
		{ }

		std::string toHostPort() const;

		const struct sockaddr_in& getSockAddrInet() const { return addr_; }
		void setSockAddrInet(const struct sockaddr_in& addr) { addr_ = addr; }

	private:
		struct sockaddr_in	addr_;
	};
}