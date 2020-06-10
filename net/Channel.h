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
		readCallback_ = cb;		//���ö��ص�����
	}

	void setWriteCallback(const EventCallback& cb)
	{
		writeCallback_ = cb;	//����д�ص�����
	}

	void setErrorCallback(const EventCallback& cb)
	{
		errorCallback_ = cb;	//���ô���ص�����
	}
	
	void setCloseCallback(const EventCallback& cb)
	{
		closeCallback_ = cb;
	}

	int  fd() const { return fd_; } //��channel�󶨵��ļ�������
	int  events() const { return events_; } //���ص�ǰ�ļ����������û������¼�
	void set_revents(int revt) { revents_ = revt; } //������ļ��������ѷ������¼�����Poller��ѭ������
	bool isNoneEvent() const { return events_ == kNoneEvent; } //�û��Ƿ񲻹��ĸ��ļ��������κ��¼�
	//ʹ�ܶ�/д�¼�����֪ͨ��ǰ�ļ�������������loop������ѯ�¼�
	void enableReading() { events_ |= kReadEvent; update(); } 
	void enableWriting() { events_ |= kWriteEvent; update(); }
	void disableWriting() { events_ &= ~kWriteEvent; update(); }

	void disableAll() { events_ = kNoneEvent; update(); }

	bool isWriting() const { return events_ & kWriteEvent; }
	//for Poller
	int index() { return index_; }
	void set_index(int index) { index_ = index; } //��poller���Աstd::vector<ChannelL*> ChannelList�е�λ��

	EventLoop* ownerLoop() { return loop_;  } //channel �������¼���

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

