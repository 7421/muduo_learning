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
		��������һ����ʱ������ͬʱ����һ����֮�������ļ���������
		int timerfd_create(int clockid,int flags);
		CLOCK_MONOTONIC: ��ʱʱ��Ӵ˿̿�ʼ
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

//�����ڿ�ʼ��ʱ���when��ʣ��ʱ��
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
//����ʱ���������������˶��ٴ�
void readTimerfd(int timerfd, Timestamp now)
{
	uint64_t howmany;
	//timerfd�����һ�λ��δ������򷵻ش����Ĵ���
	//timerfd���û�д�����������ý�����ֱ����һ����ʱ�����ڣ���������ļ��������ѱ�����Ϊ������
	//��ͨ��ʹ��fcntl��2��������ý�ʧ�ܲ���ʾ����EAGAIN ����
	// F_SETFL����������O_NONBLOCK��־����
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
	//����ӵ�ǰʱ�䵽����ʱ��(expiration)��ʱ���
	newValue.it_value = howMuchTimeFromNow(expiration);
	//�������ö�ʱ����ʱʱ�䣻����0�������������óɹ�
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
	:loop_(loop),				//TimerQueue������EventLoop
	 timerfd_(createTimerfd()),	//TimerQueue��һ��timerfd����һ����
	 timerfdChannel_(loop,timerfd_), //timerfd����channel����
	 timers_(),                  //�յĶ�ʱ�¼�����
	 callingExpiredTimers_(false)
{
	//����channel�ص�����
	timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
	//ʹ��channel���¼�����֪ͨeventloop��timerfd���뵽std::vector<Channel*> pollfds_��
	timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
	::close(timerfd_);
	/*
		typedef std::pair<Timestamp, Timer*> Entry;  
		typedef std::set<Entry> TimerList;
	*/
	//����Timers_�е����ж�ʱ��
	for (TimerList::iterator it = timers_.begin(); it != timers_.end(); ++it)
		delete it->second;
}

//ֻ����ת��
//cb: ��ʱ����ʱ�ص�������when: ��ʱʱ�䣻interval: ���ڵ��õ�ʱ����
TimerId TimerQueue::addTimer(const TimerCallback& cb, Timestamp when, double interval)
{
	Timer* timer = new Timer(cb, when, interval);

	loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop,this,timer));

	return TimerId(timer); //TimerIdΪ�ɿ�������/������ֵ����
}	

void TimerQueue::cancel(TimerId timerId)
{
	loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

//��������޸Ķ�ʱ���б���
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
	//timerfd_�����˶��ٴ��¼���û�д����¼�����һֱ�ȴ���ֱ����һ������
	readTimerfd(timerfd_, now);
	//typedef std::pair<Timestamp, Timer*> Entry;
	
	//����Ѵ����Ķ�ʱ��
	std::vector<Entry>	expired = getExpired(now);
	//����ÿ��������ʱ���Ļص�����
	for (std::vector<Entry>::iterator it = expired.begin(); it != expired.end(); ++it)
		it->second->run();
	//��������¼����ж϶�ʱ���Ƿ�Ϊ�����Եģ�
	//��Ϊ�����ԣ������¼���Ⱥ򴥷���ʱ��std::set<Entry> timers_������
	reset(expired, now);
}
//typedef std::pair<Timestamp, Timer*> Entry;  
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
	//�����Ķ�ʱ�¼�
	std::vector<Entry> expired;
	Entry sentry = std::make_pair(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
	//���������е�һ�����ڵ���sentry��λ��
	TimerList::iterator it = timers_.lower_bound(sentry);
	assert(it == timers_.end() || now < it->first);
	//�������¼�����std::vector<Entry>  expired�в�����
	std::copy(timers_.begin(), it, back_inserter(expired));
	//��timers_�ȴ�����������Ѵ����¼�
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
			//�����Զ�ʱ�����¶�ʱ���ı��䴥��ʱ��
			it->second->restart(now);
			//���µĶ�ʱ�������뵽timers_ ��
			insert(it->second);
		}
		else
		{
			delete it->second;
		}
	}
	//Ѱ��ʱ�����������Ĵ����¼�
	if (!timers_.empty())
		nextExpire = timers_.begin()->second->expiration();
	//����һ������ʱ��nextExpire��Ч�����������ö�ʱʱ��
	if (nextExpire.valid())
		resetTimerfd(timerfd_, nextExpire);
}

bool TimerQueue::insert(Timer* timer)
{                                     
	//��ǰ��ʱ������ʱ�䣬�Ƿ�ȵȴ������е�����ʱ��С��
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