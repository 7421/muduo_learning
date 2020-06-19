#include <thread>

#include "../base/AsyncLog.h"

#include "EventLoop.h"
#include "Channel.h"
#include "EPoller.h"
#include "TimerQueue.h"


#include <assert.h>
#include <poll.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <sstream>

using namespace muduo;
//__thread 线程局部变量，线程间各不干扰
__thread EventLoop* t_loopInThisThread = 0;
const int kPollTimeMs = 10000;

static int createEventfd()
{
	//内置的64位计数器初始值为0
	//EFD_NONBLOCK : 非阻塞模式
	//EFD_CLOEXEC  : 表示返回的eventfd文件描述符
	//用exec函数调用其他程序时会自动关闭这个文件描述符
	int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (evtfd < 0)
	{
		LOGE(" Failed in eventfd ");
	}
	return evtfd;
}

class IgnoreSigPipe
{
public:
	IgnoreSigPipe()
	{
		::signal(SIGPIPE, SIG_IGN);
	}
};

IgnoreSigPipe initObj;


EventLoop::EventLoop()
	: looping_(false),							//是否处在while循环
	  quit_(false),								//用于终止EventLoop 的while循环
	  callingPendingFunctors_(false),			//正在执行其他用户的回调标识
	  threadId_(std::this_thread::get_id()),	//EventLoop 所处的线程ID，用于判断是否发生跨线程调用
	  poller_(new EPoller(this)),				//EventLoop 拥有的轮询器
	  timerQueue_(new TimerQueue(this)),		//EventLoop 拥有的定时器队列
	  wakeupFd_(createEventfd()),				//创建用于wakeup的事件描述符
	  wakeupChannel_(new Channel(this, wakeupFd_)) //创建channel，根据发生的事件调用不同的函数
{
	std::ostringstream osThreadID;
	osThreadID << std::this_thread::get_id();
	LOGI("EventLoop created %p in thread % s",this, osThreadID.str().c_str());
	osThreadID.clear();
	if (t_loopInThisThread)
	{
		//同一个线程出现了两个EventLoop
		osThreadID << "Another EventLoop " << t_loopInThisThread
			<< " exists in this thread " << threadId_;
		LOGE(" %s ", osThreadID.str().c_str());
	}
	else
		t_loopInThisThread = this;
	wakeupChannel_->setReadCallback(bind(&EventLoop::handleRead, this));
	wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
	assert(!looping_);
	//析构函数，将该线程所有的EventLoop*设为空
	::close(wakeupFd_);
	t_loopInThisThread = NULL; 
}

void EventLoop::loop()
{
	assert(!looping_);
	assertInLoopThread();
	looping_ = true;
	quit_ = false;
	while (!quit_)
	{
		activeChannels_.clear();  //活动td::vector<Channel*> channel清零
		pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); //设置poll函数的超时事件，若某channel发生关心事件，则将其放入activeChannels_中
		for (ChannelList::iterator it = activeChannels_.begin();
			it != activeChannels_.end(); ++it)
			(*it)->handleEvent(pollReturnTime_);   //对每个发生事件的channel根据发生的事件调用回调函数
		doPendingFunctors();
	}
	LOGI("EventLoop %p stop looping ", this);
	looping_ = false;
}

void EventLoop::quit()
{
	quit_ = true;
	if (!isInLoopThread()) //不是loop所属线程，利用eventfd唤醒IO线程
		wakeup();
}
//在loop的IO线程内执行某个用户任务回调
void EventLoop::runInLoop(const Functor& cb)
{
	if (isInLoopThread())
		cb();
	else
		queueInLoop(cb);
}

void EventLoop::queueInLoop(const Functor& cb)
{
	//由于pendingFunctor会暴露给其他线程，先上锁
	{
		lock_guard<mutex>		lock(mutex_);
		pendingFunctors_.push_back(cb);
	}
	//如果当前线程不是IO线程，则唤醒IO线程处理事件
	if (!isInLoopThread() || callingPendingFunctors_)
		wakeup();
}


TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb)
{
	return timerQueue_->addTimer(cb, time, 0.0);
}

TimerId EventLoop::runAfter(double delay, const TimerCallback& cb)
{
	Timestamp   newTime = addTime(Timestamp::now(), int64_t(delay * 1000000));
	return runAt(newTime, cb);
}

TimerId EventLoop::runEvery(double interval, const TimerCallback& cb)
{
	Timestamp newTime = addTime(Timestamp::now(), int64_t(interval * 1000000));
	return timerQueue_->addTimer(cb, newTime, interval);
}

void EventLoop::cancel(TimerId timerId)
{
	return timerQueue_->cancel(timerId);
}


void EventLoop::updateChannel(Channel* channel)		
{
	assert(channel->ownerLoop() == this); 
	assertInLoopThread();
	poller_->updateChannel(channel); //通知poller轮循器更新channellist_列表
}

void EventLoop::removeChannel(Channel* channel)
{
	assert(channel->ownerLoop() == this);
	assertInLoopThread();
	poller_->removeChannel(channel);
}
//发生跨线程调用，打印错误信息，abort()异常退出
void EventLoop::abortNotInLoopThread()
{
	std::ostringstream os;
	os << "EventLoop::abortNotInLoopThread - EventLoop " << this
		<< " was created in threadId_ " << threadId_
		<< ", current thread id = " << std::this_thread::get_id();
	LOGE(" %s ", os.str().c_str());
}

void  EventLoop::wakeup()
{
	uint64_t one = 1;
	//向wakeupFd_写入8个字节大小的1
	ssize_t n = ::write(wakeupFd_, &one, sizeof one);
	if (n != sizeof one)
	{
		LOGE("EventLoop::wakeup() writes %d instead of 8", n);
		abort(); //程序异常退出程序，向自己发送SIGABRT信号
	}
}

void EventLoop::handleRead()
{
	uint64_t one = 1;
	ssize_t n = ::read(wakeupFd_, &one, sizeof one);
	if (n != sizeof one)
	{
		LOGE("EventLoop::wakeup() writes %d instead of 8", n);
		abort(); //程序异常退出程序，向自己发送SIGABRT信号
	}
}
//在IO线程中，执行其他用户的回调函数
void EventLoop::doPendingFunctors()
{
	std::vector<Functor>	functors;
	callingPendingFunctors_ = true;
	{
		lock_guard<mutex>		lock(mutex_);
		functors.swap(pendingFunctors_); //减少临界区复杂度，一次性将回调队列换出
	}
	//逐个执行用户回调函数
	for (size_t i = 0; i < functors.size(); ++i)
	{
		functors[i]();
	}
	callingPendingFunctors_ = false;
}

