#pragma once

#include <set>
#include <vector>

#include "../base/Timestamp.h"
#include <mutex>
#include "Callbacks.h"
#include "Channel.h"


namespace muduo
{
class EventLoop;
class Timer;
class TimerId;

class TimerQueue
{
public:
	TimerQueue(EventLoop* loop);
	~TimerQueue();

	//��ֹ��������Ϳ�����ֵ����
	TimerQueue(const TimerQueue&) = delete;
	void operator=(const TimerQueue&) = delete;

	//ֻ����ת��
	TimerId addTimer(const TimerCallback& cb,Timestamp when,double interval);

	void cancel(TimerId timerId);
private:
	//first : ����ʱ��; second :��ʱ��
	//������Ҫ���ݴ���ʱ��������򣬹�firstΪ����ʱ��
	typedef std::pair<Timestamp, Timer*> Entry;  
	typedef std::set<Entry> TimerList;
	typedef std::pair<Timer*, int64_t> ActiveTimer;
	typedef std::set<ActiveTimer> ActiveTimerSet;
	//��������޸Ķ�ʱ���б���
	void addTimerInLoop(Timer* timer);
	void cancelInLoop(TimerId timerId);
	//��ʱ�䵽��ʱ����handleRead()����
	void handleRead();

	//�����д����Ķ�ʱ�����أ�����TimerQueue�����
	std::vector<Entry> getExpired(Timestamp now);
	void reset( const std::vector<Entry> & expired, Timestamp now);

	bool insert(Timer* timer);

	EventLoop* loop_;
	const int  timerfd_;
	Channel	   timerfdChannel_;
	//std::set<Entry> ��ʱ���������ݴ���ʱ����������
	TimerList  timers_;

	//for cancel 
	bool callingExpiredTimers_;
	ActiveTimerSet activeTimers_;
	ActiveTimerSet cancelingTimers_;
};


}