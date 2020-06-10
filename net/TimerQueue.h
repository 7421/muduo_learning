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

	//禁止拷贝构造和拷贝赋值操作
	TimerQueue(const TimerQueue&) = delete;
	void operator=(const TimerQueue&) = delete;

	//只负责转发
	TimerId addTimer(const TimerCallback& cb,Timestamp when,double interval);

	void cancel(TimerId timerId);
private:
	//first : 触发时间; second :定时器
	//由于需要根据触发时间逐个排序，故first为触发时间
	typedef std::pair<Timestamp, Timer*> Entry;  
	typedef std::set<Entry> TimerList;
	typedef std::pair<Timer*, int64_t> ActiveTimer;
	typedef std::set<ActiveTimer> ActiveTimerSet;
	//负责完成修改定时器列表工作
	void addTimerInLoop(Timer* timer);
	void cancelInLoop(TimerId timerId);
	//当时间到达时调用handleRead()函数
	void handleRead();

	//将所有触发的定时器返回，并在TimerQueue中清除
	std::vector<Entry> getExpired(Timestamp now);
	void reset( const std::vector<Entry> & expired, Timestamp now);

	bool insert(Timer* timer);

	EventLoop* loop_;
	const int  timerfd_;
	Channel	   timerfdChannel_;
	//std::set<Entry> 定时器容器根据触发时间升序排列
	TimerList  timers_;

	//for cancel 
	bool callingExpiredTimers_;
	ActiveTimerSet activeTimers_;
	ActiveTimerSet cancelingTimers_;
};


}