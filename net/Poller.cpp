#include "Poller.h"

#include "Channel.h"
#include <iostream>

#include <assert.h>
#include <poll.h>

using namespace muduo;

Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop)  //初始化轮循器所属事件环
{
}

Poller::~Poller()
{
}
//轮询，调用poll函数由内核告诉我们，发生了什么事件
Timestamp Poller::poll(int timeoutMs, ChannelList* activeChannels)
{
    //轮询，若发生我们关心的事件，则返回相应的事件数目
    int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);
    Timestamp now(Timestamp::now());
    if (numEvents > 0)//大于0，表示准备就绪的描述符数目
    {
        std::cout << numEvents << " events happended" << endl;
        //将发生的事件放入activeChannels数组中
        fillActiveChannels(numEvents, activeChannels);
    }
    else if (numEvents == 0) //超时返回0
        std::cout << " nothing happended" << std::endl;
    else   //出错，返回-1
    {
        std::cerr << "Poller::poll()" << std::endl;
        abort();
    }
    return now;
}
/*
    struct pollfd{
        int     fd;         //文件描述符
        short   events;     //用户关心事件
        short   revents;    //由内核设置的已发生事件
    };
*/

//将已发生我们关心的事件放入activeChannels数组中
void Poller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    for (PollFdList::const_iterator pfd = pollfds_.begin(); pfd != pollfds_.end() && numEvents > 0; ++pfd)
    {
        if (pfd->revents > 0)
        {
            --numEvents;
            ChannelMap::const_iterator ch = channels_.find(pfd->fd);
            assert(ch != channels_.end());
            Channel* channel = ch->second;
            assert(channel->fd() == pfd->fd);
            channel->set_revents(pfd->revents); //将本文件描述符已发生事件保存在channel中
            activeChannels->push_back(channel); //已发生事件放入activeChannels数组中
        }
    }
}
//更新渠道channel
void Poller::updateChannel(Channel* channel)
{
    assertInLoopThread();
    std::cout << "fd = " << channel->fd() << " events = " << channel->events() << std::endl;
    if (channel->index() < 0) //index_初始化为-1，表示其绑定的文件描述符在PollFdList数组中的坐标
    {
        std::cout << channels_.size() << std::endl;
        //channel不在轮询器中，将其放入std::vector<Channel*> pollfds_数组中
        assert(channels_.find(channel->fd()) == channels_.end());
        struct pollfd  pfd;
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        pollfds_.push_back(pfd);
        int idx = static_cast<int>(pollfds_.size()) - 1;//新加入的文件描述符在PollFdList数组中的坐标
        channel->set_index(idx);
        channels_[pfd.fd] = channel;
    }
    else
    {
        //channel已存在轮询器中，更新现有的数据
        assert(channels_.find(channel->fd()) != channels_.end());
        assert(channels_[channel->fd()] == channel);

        int idx = channel->index();
        assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
        struct pollfd& pfd = pollfds_[idx];
        assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd() - 1);
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        if (channel->isNoneEvent())
            //channel绑定的fd文件描述符无事件
            pfd.fd = -channel->fd() - 1;
    }
}

//删除channel
void Poller::removeChannel(Channel* channel)
{
    assertInLoopThread();
    std::cout << "fd = " << channel->fd();
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);
    assert(channel->isNoneEvent());

    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
    const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
    assert(pfd.fd == -channel->fd() - 1 && pfd.events == channel->events());
    size_t n = channels_.erase(channel->fd());
    assert(n == 1); (void)n;
    if (static_cast<size_t>(idx) == pollfds_.size() - 1)
    {
        pollfds_.pop_back();
    }
    else
    {
        int channelAtEnd = pollfds_.back().fd;
        iter_swap(pollfds_.begin() + idx, pollfds_.end() - 1);
        if (channelAtEnd < 0)
            channelAtEnd = -channelAtEnd - 1;;
        channels_[channelAtEnd]->set_index(idx);
        pollfds_.pop_back();
    }
}