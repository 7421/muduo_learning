#pragma once

namespace muduo
{
class InetAddress;

class Socket
{
public:
	explicit Socket(int sockfd)
		: sockfd_(sockfd)
	{}

	~Socket();

	Socket(const Socket&) = delete;
	void operator=(const Socket&) = delete; 

	int fd() const { return sockfd_;  }

	void bindAddress(const InetAddress& localaddr);

	/// On success, returns a non-negative integer that is
	/// a descriptor for the accepted socket, which has been
	/// set to non-blocking and close-on-exec. *peeraddr is assigned.
	/// On error, -1 is returned, and *peeraddr is untouched.
	void listen();

	int accept(InetAddress* peeraddr);

	// Enable/disable SO_REUSEADDR
	void setReuseAddr(bool on);

	void shutdownWrite();

	///
	/// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
	///
	void setTcpNoDelay(bool on);
private:
	const int sockfd_;
};
}