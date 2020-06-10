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

	//��ѯI/O�¼����ṩ��Eventloop�̵߳���
	Timestamp poll(int timeoutMs, ChannelList* activeChannels);

	//�ı���ӵ�I/O�¼�
	//��loop�̵߳���
	void updateChannel(Channel* channel);
	// ��channel����ʱ��ɾ��channel����
	// ��IO�̵߳���
	void removeChannel(Channel* channel);

	void assertInLoopThread() { ownerLoop_->assertInLoopThread(); }
private:
	void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

	typedef std::vector<struct pollfd> PollFdList;
	typedef std::map<int, Channel*> ChannelMap;

	EventLoop* ownerLoop_;	//��ѯ��������EventLoop
	PollFdList pollfds_;	//��ѯ�����ӵ�pollfd����
	ChannelMap channels_;
};
}

