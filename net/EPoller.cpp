#include "EPoller.h"



#include <iostream>

#include <assert.h> 
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include <memory.h>

#include "EventLoop.h"
#include "Channel.h"

using namespace muduo;

//匿名空间，名称的作用域被限制在当前文件中
namespace
{
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
}

EPoller::EPoller(EventLoop* loop)
	: ownerLoop_(loop),
	epollfd_(::epoll_create(EPOLL_CLOEXEC)),
	events_(kInitEventListSize)
{
	if (epollfd_ < 0)
	{
		std::cerr << "Epoller::Epoller" << std::endl;
		abort();
	}
}

EPoller::~EPoller()
{
	::CloseCallback(epoll_fd);
}

Timestamp EPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
	int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
	Timestamp now(Timestamp::now());

	if (numEvents > 0)
	{
		std::cout << numEvents << " events happended" << std::endl;
		fillActiveChannels(numEvents, activeChannels);
		if (static_cast<size_t>(numEvents) == events_.size())
		{
			events_.resize(events_.size() * 2);
		}
	}
	else if (numEvents == 0)
	{
		std::cout << " nothing happended " << std::endl;
	}
	else
	{
		std::cout << "EPoller::poll()" << std::endl;
		abort();
	}
	return now;
}

void EPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
	assert(static_cast<size_t>(numEvents) <= events_.size());
	for (int i = 0; i < numEvents; ++i)
	{
		Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
		int fd = channel->fd();
		ChannelMap::const_iterator it = channels_.find(fd);
		assert(it != channels_.end());
		assert(it->second == channel);

		channel->set_revents(events_[i].events);
		activeChannels->push_back(channel);
	}
}

void EPoller::updateChannel(Channel* channel)
{
	assertInLoopThread();
	std::cout << "fd = " << channel->fd() << " events = " << channel->events() << std::endl;   
	const int index = channel->index();

	if (index == kNew || index == kDeleted)
	{
		// a new one, add with EPOLL_CTL_ADD
		int fd = channel->fd();
		if (index == kNew)
		{
			assert(channels_.find(fd) == channels_.end());
			channels_[fd] = channel;
		}
		else  //index == kDeleted
		{
			assert(channels_.find(fd) != channels_.end());
			assert(channels_[fd] == channel);
		}
		channel->set_index(kAdded);
		update(EPOLL_CTL_ADD, channel);
	}
	else
	{
		//update existing one with EPOLL_CTL_MOD/DEL
		int fd = channel->fd();
		(void)fd;
		assert(channels_.find(fd) != channels_.end());
		assert(channels_[fd] == channel);
		assert(index == kAdded);

		if (channel->isNoneEvent())
		{
			update(EPOLL_CTL_DEL, channel);
			channel->set_index(kDeleted);
		}
		else
			update(EPOLL_CTL_MOD, channel);
	}
}

void EPoller::removeChannel(Channel* channel)
{
	assertInLoopThread();
	int fd = channel->fd();
	std::cout << " fd = " << fd;
	assert(channels_.find(fd) != channels_.end());
	assert(channels_[fd] == channel);
	assert(channel->isNoneEvent());

	int index = channel->index();
	assert(index == kAdded || index == kDeleted);
	size_t n = channels_.erase(fd);
	(void)n;
	assert(n == 1);

	if (index == kAdded)
	{
		update(EPOLL_CTL_DEL, channel);
	}
	channel->set_index(kNew);
}


void EPoller::update(int operation, Channel* channel)
{
	struct epoll_event event;
	bzero(&event, sizeof event);
	event.events = channel->events();
	event.data.ptr = channel;
	int fd = channel->fd();
	if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
	{
		if (operation == EPOLL_CTL_DEL)
		{
			std::cerr << "epoll_ctl op=" << operation << " fd=" << fd << std::endl;
		}
		else
		{
			std::cerr << "epoll_ctl op=" << operation << " fd=" << fd << std::endl;
		}
		abort();
	}
}