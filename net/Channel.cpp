#include "Channel.h"
#include "EventLoop.h"

#include "../base/Timestamp.h"
#include "../base/AsyncLog.h"

#include <poll.h>
#include <assert.h>

using namespace muduo;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI; //POLLIN(普通数据和优先数据可读)，POLLPRI（高优先数据可读）
const int Channel::kWriteEvent = POLLOUT; //POLLOUT(普通数据可写）

Channel::Channel(EventLoop* loop, int fdArg)
    : loop_(loop),
    fd_(fdArg),
    events_(0),  //在fd文件描述符上，准备监听的事件
    revents_(0), //在fd文件描述符上，已经发生的事件
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
    //通知所属事件环，更新Channel列表
    loop_->updateChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    eventHandling_ = true;
    //描述符没用引用一个打开的文件
    if (revents_ & POLLNVAL)
    {
       LOGI(" Channel::handle_event() POLLNVAL "); 
    }
    //出现挂起事件
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        LOGI( "Channel::handle_event() POLLHUP");
        if (closeCallback_) closeCallback_();
    }
    //发生错误事件，调用错误回调函数
    if (revents_ & (POLLERR | POLLNVAL))
        if (errorCallback_) errorCallback_();
    //发生POLLIN：普通读事件、优先读事件, POLLPRI: 高优先级可读，POLLRDHUP：TCP连接被对端关闭，或者关闭了写操作
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) 
        if(readCallback_) readCallback_(receiveTime);
    //发生POLLOUT: 普通数据可写事件
    if (revents_ & POLLOUT)
        if (writeCallback_) writeCallback_();
    eventHandling_ = false;
}


