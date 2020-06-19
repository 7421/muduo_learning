#include "Channel.h"
#include "EventLoop.h"

#include "../base/Timestamp.h"
#include "../base/AsyncLog.h"

#include <poll.h>
#include <assert.h>

using namespace muduo;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI; //POLLIN(��ͨ���ݺ��������ݿɶ�)��POLLPRI�����������ݿɶ���
const int Channel::kWriteEvent = POLLOUT; //POLLOUT(��ͨ���ݿ�д��

Channel::Channel(EventLoop* loop, int fdArg)
    : loop_(loop),
    fd_(fdArg),
    events_(0),  //��fd�ļ��������ϣ�׼���������¼�
    revents_(0), //��fd�ļ��������ϣ��Ѿ��������¼�
    index_(-1),
    eventHandling_(false)
{

}


Channel::~Channel()
{
    assert(!eventHandling_);
}


void Channel::update()
{
    //֪ͨ�����¼���������Channel�б�
    loop_->updateChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    eventHandling_ = true;
    //������û������һ���򿪵��ļ�
    if (revents_ & POLLNVAL)
    {
       LOGI(" Channel::handle_event() POLLNVAL "); 
    }
    //���ֹ����¼�
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        LOGI( "Channel::handle_event() POLLHUP");
        if (closeCallback_) closeCallback_();
    }
    //���������¼������ô���ص�����
    if (revents_ & (POLLERR | POLLNVAL))
        if (errorCallback_) errorCallback_();
    //����POLLIN����ͨ���¼������ȶ��¼�, POLLPRI: �����ȼ��ɶ���POLLRDHUP��TCP���ӱ��Զ˹رգ����߹ر���д����
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) 
        if(readCallback_) readCallback_(receiveTime);
    //����POLLOUT: ��ͨ���ݿ�д�¼�
    if (revents_ & POLLOUT)
        if (writeCallback_) writeCallback_();
    eventHandling_ = false;
}


