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

	EventLoop(const EventLoop&) = delete;		//禁止默认的拷贝构造函数
	void operator=(const EventLoop&) = delete;	//禁止默认的拷贝赋值函数



	void loop();

	void quit();

	//Time when poll returns, usually means data arrivial.
	Timestamp pollReturnTime() const { return pollReturnTime_; }
	
	//如果用户在当前IO线程调用这个函数，回调会同步进行
	//如果用户在其他线程调用runInLoop，cb会被加入队列IO线程会被唤醒来调用这个Functor
	void runInLoop(const Functor& cb);
	
	/// Queues callback in the loop thread.
	/// Runs after finish pooling.
	/// Safe to call from other threads.
	void queueInLoop(const Functor& cb);

	//在指定时间执行回调函数cb
	TimerId runAt(const Timestamp& time, const TimerCallback& cb);
	//在delay秒后执行回调函数cb
	TimerId runAfter(double delay, const TimerCallback& cb);
	//间隔interval周期秒执行回调函数cb
	TimerId runEvery(double interval, const TimerCallback& cb);

	void cancel(TimerId timerId);

	//只在内部使用
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

	bool quit_;					//停止标志，用于其他线程停止while循环
	bool looping_;				//处于while循环中的标识
	bool callingPendingFunctors_;
	const std::thread::id		threadId_; //loop所处的线程ID
	Timestamp					pollReturnTime_;//poll轮询返回的时间
	std::unique_ptr<EPoller>     poller_;    //EventLoop管理的poller_l轮询器 
	std::unique_ptr<TimerQueue>	timerQueue_;//EventLoop管理的定时队列
	int wakeupFd_;							//用于唤醒IO线程	
									
	std::unique_ptr<Channel>	wakeupChannel_;  //用于唤醒事件的channel
	ChannelList					activeChannels_; //用于存放已触发事件的信道channel
	std::mutex					mutex_;
	std::vector<Functor>		pendingFunctors_;
};
}
