#include "TimerQueue.h"

#include <functional>
#include <algorithm>

#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"
#include "../base/AsyncLog.h"

#include <unistd.h>
#include <sys/timerfd.h>
#include <memory.h>
#include <assert.h>

namespace muduo
{
namespace detail
{

int createTimerfd()
{
	/*
		函数创建一个定时器对象，同时返回一个与之关联的文件描述符。
		int timerfd_create(int clockid,int flags);
		CLOCK_MONOTONIC: 定时时间从此刻开始
	*/
	int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	
	if (timerfd < 0)
	{
		LOGE(" Failed in timerfd_create ");
	}
	return timerfd;
}

/*
	struct timespec{
		time_t	tv_sec;
		long    tv_nsec;
	}
*/

//从现在开始到时间戳when的剩余时间
struct timespec howMuchTimeFromNow(Timestamp when)
{
	int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
	if (microseconds < 100)
		microseconds = 100;
	struct timespec ts;
	ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
	ts.tv_nsec = static_cast<long>((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
	return ts;
}
//读定时器描述符，发生了多少次
void readTimerfd(int timerfd, Timestamp now)
{
	uint64_t howmany;
	//timerfd如果有一次或多次触发，则返回触发的次数
	//timerfd如果没有触发过，则调用将阻塞直到下一个计时器到期，或者如果文件描述符已被设置为非阻塞
	//（通过使用fcntl（2），则调用将失败并显示错误EAGAIN ）。
	// F_SETFL操作来设置O_NONBLOCK标志）。
	ssize_t  n = ::read(timerfd, &howmany, sizeof howmany);
	LOGI(" TimerQueue::handleRead() %d at %s", howmany, now.toString().c_str());
	if (n != sizeof howmany)
	{
		LOGE(" TimerQueue::handleRead() reads %d bytes instead of 8",n);
	}
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
	struct itimerspec newValue;
	struct itimerspec oldValue;

	memset(&newValue, 0,sizeof newValue);
	memset(&oldValue, 0, sizeof oldValue);
	//计算从当前时间到触发时间(expiration)的时间差
	newValue.it_value = howMuchTimeFromNow(expiration);
	//重新设置定时器定时时间；返回0，代表重新设置成功
	int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
	if (ret)
	{
		LOGE(" timerfd_settime() ");
	}
}
}

using namespace muduo;
using namespace muduo::detail;

TimerQueue::TimerQueue(EventLoop* loop)
	:loop_(loop),				//TimerQueue所属的EventLoop
	 timerfd_(createTimerfd()),	//TimerQueue由一个timerfd来逐一触发
	 timerfdChannel_(loop,timerfd_), //timerfd所属channel渠道
	 timers_(),                  //空的定时事件集合
	 callingExpiredTimers_(false)
{
	//设置channel回调函数
	timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
	//使能channel读事件，并通知eventloop将timerfd加入到std::vector<Channel*> pollfds_中
	timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
	::close(timerfd_);
	/*
		typedef std::pair<Timestamp, Timer*> Entry;  
		typedef std::set<Entry> TimerList;
	*/
	//析构Timers_中的所有定时器
	for (TimerList::iterator it = timers_.begin(); it != timers_.end(); ++it)
		delete it->second;
}

//只负责转发
//cb: 定时器超时回调函数；when: 定时时间；interval: 周期调用的时间间隔
TimerId TimerQueue::addTimer(const TimerCallback& cb, Timestamp when, double interval)
{
	Timer* timer = new Timer(cb, when, interval);

	loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop,this,timer));

	return TimerId(timer); //TimerId为可拷贝构造/拷贝赋值函数
}	

void TimerQueue::cancel(TimerId timerId)
{
	loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

//负责完成修改定时器列表工作
void TimerQueue::addTimerInLoop(Timer* timer)
{
	loop_->assertInLoopThread();
	bool earliestChanged = insert(timer);
	if (earliestChanged)
	{
		resetTimerfd(timerfd_, timer->expiration());
	}
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
	loop_->assertInLoopThread();
	assert(timers_.size() == activeTimers_.size());
	ActiveTimer timer(timerId.timer_, timerId.sequence_);
	ActiveTimerSet::iterator it = activeTimers_.find(timer);
	if (it != activeTimers_.end())
	{
		size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
		assert(n == 1); (void)n;
		delete  it->first;
		activeTimers_.erase(it);
	}
	else if (callingExpiredTimers_)
	{
		cancelingTimers_.insert(timer);
	}
	assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
	loop_->assertInLoopThread();
	Timestamp now(Timestamp::now());
	//timerfd_发生了多少次事件，没有触发事件，会一直等待，直到第一个触发
	readTimerfd(timerfd_, now);
	//typedef std::pair<Timestamp, Timer*> Entry;
	
	//获得已触发的定时器
	std::vector<Entry>	expired = getExpired(now);
	//调用每个触发定时器的回调函数
	for (std::vector<Entry>::iterator it = expired.begin(); it != expired.end(); ++it)
		it->second->run();
	//处理完读事件后，判断定时器是否为周期性的，
	//若为周期性，则重新加入等候触发定时器std::set<Entry> timers_集合中
	reset(expired, now);
}
//typedef std::pair<Timestamp, Timer*> Entry;  
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
	//触发的定时事件
	std::vector<Entry> expired;
	Entry sentry = std::make_pair(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
	//返回数组中第一个大于等于sentry的位置
	TimerList::iterator it = timers_.lower_bound(sentry);
	assert(it == timers_.end() || now < it->first);
	//将触发事件放入std::vector<Entry>  expired中并返回
	std::copy(timers_.begin(), it, back_inserter(expired));
	//从timers_等待容器中清除已触发事件
	//std::set<Entry> timers_
	timers_.erase(timers_.begin(), it);

	for (auto entry : expired)
	{
		ActiveTimer timer(entry.second, entry.second->sequence());
		size_t n = activeTimers_.erase(timer);
		assert(n == 1); (void)n;
	}
	assert(timers_.size() == activeTimers_.size());
	return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
	Timestamp nextExpire;
	for (std::vector<Entry>::const_iterator it = expired.begin(); it != expired.end(); ++it)
	{
		if (it->second->repeat())
		{
			//周期性定时器重新定时，改变其触发时间
			it->second->restart(now);
			//将新的定时器，加入到timers_ 中
			insert(it->second);
		}
		else
		{
			delete it->second;
		}
	}
	//寻找时间队列中最近的触发事件
	if (!timers_.empty())
		nextExpire = timers_.begin()->second->expiration();
	//若下一个触发时间nextExpire有效，则重新设置定时时间
	if (nextExpire.valid())
		resetTimerfd(timerfd_, nextExpire);
}

bool TimerQueue::insert(Timer* timer)
{                                     
	//当前定时器触发时间，是否比等待队列中的所有时间小。
	bool		earliestChanged = false;
	Timestamp	when = timer->expiration();
	TimerList::iterator it = timers_.begin();
	//std::set<Entry> timers_
	if (it == timers_.end() || when < it->first)
		earliestChanged = true;
	{
		std::pair<TimerList::iterator, bool> result
			= timers_.insert(Entry(when, timer));
		assert(result.second); (void)result;
	}
	{
		std::pair<ActiveTimerSet::iterator, bool> result
			= activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
		assert(result.second); (void)result;
	}
	assert(timers_.size() == activeTimers_.size());
	return earliestChanged;
}


}