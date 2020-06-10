#include "Poller.h"

#include "Channel.h"
#include <iostream>

#include <assert.h>
#include <poll.h>

using namespace muduo;

Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop)  //��ʼ����ѭ�������¼���
{
}

Poller::~Poller()
{
}
//��ѯ������poll�������ں˸������ǣ�������ʲô�¼�
Timestamp Poller::poll(int timeoutMs, ChannelList* activeChannels)
{
    //��ѯ�����������ǹ��ĵ��¼����򷵻���Ӧ���¼���Ŀ
    int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);
    Timestamp now(Timestamp::now());
    if (numEvents > 0)//����0����ʾ׼����������������Ŀ
    {
        std::cout << numEvents << " events happended" << endl;
        //���������¼�����activeChannels������
        fillActiveChannels(numEvents, activeChannels);
    }
    else if (numEvents == 0) //��ʱ����0
        std::cout << " nothing happended" << std::endl;
    else   //��������-1
    {
        std::cerr << "Poller::poll()" << std::endl;
        abort();
    }
    return now;
}
/*
    struct pollfd{
        int     fd;         //�ļ�������
        short   events;     //�û������¼�
        short   revents;    //���ں����õ��ѷ����¼�
    };
*/

//���ѷ������ǹ��ĵ��¼�����activeChannels������
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
            channel->set_revents(pfd->revents); //�����ļ��������ѷ����¼�������channel��
            activeChannels->push_back(channel); //�ѷ����¼�����activeChannels������
        }
    }
}
//��������channel
void Poller::updateChannel(Channel* channel)
{
    assertInLoopThread();
    std::cout << "fd = " << channel->fd() << " events = " << channel->events() << std::endl;
    if (channel->index() < 0) //index_��ʼ��Ϊ-1����ʾ��󶨵��ļ���������PollFdList�����е�����
    {
        std::cout << channels_.size() << std::endl;
        //channel������ѯ���У��������std::vector<Channel*> pollfds_������
        assert(channels_.find(channel->fd()) == channels_.end());
        struct pollfd  pfd;
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        pollfds_.push_back(pfd);
        int idx = static_cast<int>(pollfds_.size()) - 1;//�¼�����ļ���������PollFdList�����е�����
        channel->set_index(idx);
        channels_[pfd.fd] = channel;
    }
    else
    {
        //channel�Ѵ�����ѯ���У��������е�����
        assert(channels_.find(channel->fd()) != channels_.end());
        assert(channels_[channel->fd()] == channel);

        int idx = channel->index();
        assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
        struct pollfd& pfd = pollfds_[idx];
        assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd() - 1);
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        if (channel->isNoneEvent())
            //channel�󶨵�fd�ļ����������¼�
            pfd.fd = -channel->fd() - 1;
    }
}

//ɾ��channel
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