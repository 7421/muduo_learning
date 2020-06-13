#pragma once

#include "../base/Timestamp.h"
#include "Callbacks.h"
#include "TimerId.h"

#include <thread>
#include <poll.h>
#include <vector>
#include <functional>
#include <mutex>

namespace muduo
{

class EPoller;
class Channel;
class TimerQueue;

class EventLoop
{
public:
	typedef function<void()> Functor;

	EventLoop();
	~EventLoop();

	EventLoop(const EventLoop&) = delete;		//��ֹĬ�ϵĿ������캯��
	void operator=(const EventLoop&) = delete;	//��ֹĬ�ϵĿ�����ֵ����



	void loop();

	void quit();

	//Time when poll returns, usually means data arrivial.
	Timestamp pollReturnTime() const { return pollReturnTime_; }
	
	//����û��ڵ�ǰIO�̵߳�������������ص���ͬ������
	//����û��������̵߳���runInLoop��cb�ᱻ�������IO�̻߳ᱻ�������������Functor
	void runInLoop(const Functor& cb);
	
	/// Queues callback in the loop thread.
	/// Runs after finish pooling.
	/// Safe to call from other threads.
	void queueInLoop(const Functor& cb);

	//��ָ��ʱ��ִ�лص�����cb
	TimerId runAt(const Timestamp& time, const TimerCallback& cb);
	//��delay���ִ�лص�����cb
	TimerId runAfter(double delay, const TimerCallback& cb);
	//���interval������ִ�лص�����cb
	TimerId runEvery(double interval, const TimerCallback& cb);

	void cancel(TimerId timerId);

	//ֻ���ڲ�ʹ��
	void wakeup();
	void updateChannel(Channel* channel);
	void removeChannel(Channel* channel);
	
	void assertInLoopThread()
	{
		if (!isInLoopThread())
		{
			abortNotInLoopThread();
		}
	}

	bool isInLoopThread() const { return threadId_ == std::this_thread::get_id();}

private:
	typedef std::vector<Channel*> ChannelList;

	void abortNotInLoopThread();
	void handleRead();			//waked up
	void doPendingFunctors();

	bool quit_;					//ֹͣ��־�����������߳�ֹͣwhileѭ��
	bool looping_;				//����whileѭ���еı�ʶ
	bool callingPendingFunctors_;
	const std::thread::id		threadId_; //loop�������߳�ID
	Timestamp					pollReturnTime_;//poll��ѯ���ص�ʱ��
	std::unique_ptr<EPoller>     poller_;    //EventLoop�����poller_l��ѯ�� 
	std::unique_ptr<TimerQueue>	timerQueue_;//EventLoop����Ķ�ʱ����
	int wakeupFd_;							//���ڻ���IO�߳�	
									
	std::unique_ptr<Channel>	wakeupChannel_;  //���ڻ����¼���channel
	ChannelList					activeChannels_; //���ڴ���Ѵ����¼����ŵ�channel
	std::mutex					mutex_;
	std::vector<Functor>		pendingFunctors_;
};
}
