#pragma once

#include <functional>
#include "../base/Timestamp.h"
namespace muduo
{
class EventLoop;

class Channel
{
public:
	typedef	std::function<void()>	EventCallback;
	typedef std::function<void(Timestamp)> ReadEventCallback;

	Channel(EventLoop* loop, int fd);
	~Channel();

	void handleEvent(Timestamp receiveTime);
	void setReadCallback(const ReadEventCallback& cb)
	{
		readCallback_ = cb;		//设置读回调函数
	}

	void setWriteCallback(const EventCallback& cb)
	{
		writeCallback_ = cb;	//设置写回调函数
	}

	void setErrorCallback(const EventCallback& cb)
	{
		errorCallback_ = cb;	//设置错误回调函数
	}
	
	void setCloseCallback(const EventCallback& cb)
	{
		closeCallback_ = cb;
	}

	int  fd() const { return fd_; } //与channel绑定的文件描述符
	int  events() const { return events_; } //返回当前文件描述符，用户关心事件
	void set_revents(int revt) { revents_ = revt; } //保存该文件描述符已发生的事件，由Poller轮循器调用
	bool isNoneEvent() const { return events_ == kNoneEvent; } //用户是否不关心该文件描述符任何事件
	//使能读/写事件，并通知当前文件描述符所属的loop更新轮询事件
	void enableReading() { events_ |= kReadEvent; update(); } 
	void enableWriting() { events_ |= kWriteEvent; update(); }
	void disableWriting() { events_ &= ~kWriteEvent; update(); }

	void disableAll() { events_ = kNoneEvent; update(); }

	bool isWriting() const { return events_ & kWriteEvent; }
	//for Poller
	int index() { return index_; }
	void set_index(int index) { index_ = index; } //在poller类成员std::vector<ChannelL*> ChannelList中的位置

	EventLoop* ownerLoop() { return loop_;  } //channel 所属的事件环

private:
	void update();

	static const int kNoneEvent;
	static const int kReadEvent;
	static const int kWriteEvent;


	EventLoop* loop_;
	const int  fd_;
	int		   events_;
	int		   revents_;
	int		   index_; // used by Poller.

	bool       eventHandling_;

	ReadEventCallback	readCallback_;
	EventCallback	writeCallback_;
	EventCallback	errorCallback_;
	EventCallback	closeCallback_;
};
}

