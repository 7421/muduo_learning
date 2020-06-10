#pragma once

#include <map>
#include <vector>

#include "../base/Timestamp.h"
#include "EventLoop.h"

struct pollfd;

namespace muduo
{
class Channel;

class Poller
{
public: 
	typedef std::vector<Channel*> ChannelList;

	Poller(EventLoop* loop);
	~Poller();

	//轮询I/O事件，提供给Eventloop线程调用
	Timestamp poll(int timeoutMs, ChannelList* activeChannels);

	//改变监视的I/O事件
	//由loop线程调用
	void updateChannel(Channel* channel);
	// 当channel析构时，删除channel渠道
	// 由IO线程调用
	void removeChannel(Channel* channel);

	void assertInLoopThread() { ownerLoop_->assertInLoopThread(); }
private:
	void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

	typedef std::vector<struct pollfd> PollFdList;
	typedef std::map<int, Channel*> ChannelMap;

	EventLoop* ownerLoop_;	//轮询器所属的EventLoop
	PollFdList pollfds_;	//轮询器监视的pollfd数组
	ChannelMap channels_;
};
}

