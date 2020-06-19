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
//__thread �ֲ߳̾��������̼߳��������
__thread EventLoop* t_loopInThisThread = 0;
const int kPollTimeMs = 10000;

static int createEventfd()
{
	//���õ�64λ��������ʼֵΪ0
	//EFD_NONBLOCK : ������ģʽ
	//EFD_CLOEXEC  : ��ʾ���ص�eventfd�ļ�������
	//��exec����������������ʱ���Զ��ر�����ļ�������
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
	: looping_(false),							//�Ƿ���whileѭ��
	  quit_(false),								//������ֹEventLoop ��whileѭ��
	  callingPendingFunctors_(false),			//����ִ�������û��Ļص���ʶ
	  threadId_(std::this_thread::get_id()),	//EventLoop �������߳�ID�������ж��Ƿ������̵߳���
	  poller_(new EPoller(this)),				//EventLoop ӵ�е���ѯ��
	  timerQueue_(new TimerQueue(this)),		//EventLoop ӵ�еĶ�ʱ������
	  wakeupFd_(createEventfd()),				//��������wakeup���¼�������
	  wakeupChannel_(new Channel(this, wakeupFd_)) //����channel�����ݷ������¼����ò�ͬ�ĺ���
{
	std::ostringstream osThreadID;
	osThreadID << std::this_thread::get_id();
	LOGI("EventLoop created %p in thread % s",this, osThreadID.str().c_str());
	osThreadID.clear();
	if (t_loopInThisThread)
	{
		//ͬһ���̳߳���������EventLoop
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
	//���������������߳����е�EventLoop*��Ϊ��
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
		activeChannels_.clear();  //�td::vector<Channel*> channel����
		pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); //����poll�����ĳ�ʱ�¼�����ĳchannel���������¼����������activeChannels_��
		for (ChannelList::iterator it = activeChannels_.begin();
			it != activeChannels_.end(); ++it)
			(*it)->handleEvent(pollReturnTime_);   //��ÿ�������¼���channel���ݷ������¼����ûص�����
		doPendingFunctors();
	}
	LOGI("EventLoop %p stop looping ", this);
	looping_ = false;
}

void EventLoop::quit()
{
	quit_ = true;
	if (!isInLoopThread()) //����loop�����̣߳�����eventfd����IO�߳�
		wakeup();
}
//��loop��IO�߳���ִ��ĳ���û�����ص�
void EventLoop::runInLoop(const Functor& cb)
{
	if (isInLoopThread())
		cb();
	else
		queueInLoop(cb);
}

void EventLoop::queueInLoop(const Functor& cb)
{
	//����pendingFunctor�ᱩ¶�������̣߳�������
	{
		lock_guard<mutex>		lock(mutex_);
		pendingFunctors_.push_back(cb);
	}
	//�����ǰ�̲߳���IO�̣߳�����IO�̴߳����¼�
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
	poller_->updateChannel(channel); //֪ͨpoller��ѭ������channellist_�б�
}

void EventLoop::removeChannel(Channel* channel)
{
	assert(channel->ownerLoop() == this);
	assertInLoopThread();
	poller_->removeChannel(channel);
}
//�������̵߳��ã���ӡ������Ϣ��abort()�쳣�˳�
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
	//��wakeupFd_д��8���ֽڴ�С��1
	ssize_t n = ::write(wakeupFd_, &one, sizeof one);
	if (n != sizeof one)
	{
		LOGE("EventLoop::wakeup() writes %d instead of 8", n);
		abort(); //�����쳣�˳��������Լ�����SIGABRT�ź�
	}
}

void EventLoop::handleRead()
{
	uint64_t one = 1;
	ssize_t n = ::read(wakeupFd_, &one, sizeof one);
	if (n != sizeof one)
	{
		LOGE("EventLoop::wakeup() writes %d instead of 8", n);
		abort(); //�����쳣�˳��������Լ�����SIGABRT�ź�
	}
}
//��IO�߳��У�ִ�������û��Ļص�����
void EventLoop::doPendingFunctors()
{
	std::vector<Functor>	functors;
	callingPendingFunctors_ = true;
	{
		lock_guard<mutex>		lock(mutex_);
		functors.swap(pendingFunctors_); //�����ٽ������Ӷȣ�һ���Խ��ص����л���
	}
	//���ִ���û��ص�����
	for (size_t i = 0; i < functors.size(); ++i)
	{
		functors[i]();
	}
	callingPendingFunctors_ = false;
}

