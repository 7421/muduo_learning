#include "EventLoopThread.h"
#include "EventLoop.h"
#include <functional>
#include <thread>
#include <mutex>

using namespace muduo;

EventLoopThread::EventLoopThread()
	: loop_(NULL),		//由子线程创建的EventLoop*
	  exiting_(false),  
	  thread_(),		//一个空的thread对象,本身不具有任何线程
	  mutex_(),		    //std::mutex		mutex_; 互斥锁
	  cond_()			//std::condition_variable 条件变量
{
}

EventLoopThread::~EventLoopThread()
{
	exiting_ = true;
	loop_->quit();
	thread_.join();
}

EventLoop* EventLoopThread::startLoop()
{
	thread_ = std::thread(bind(&EventLoopThread::threadFunc, this));
	{
		std::unique_lock<std::mutex>  lock(mutex_);
		//由于系统中断，条件变量存在虚假唤醒，需要再次检测，loop是否创建完成
		while (loop_ == NULL)
		{
			cond_.wait(lock);   //由于条件不合适释放锁，等待条件合适再次获得锁
		}
	}
	return loop_;
}

void EventLoopThread::threadFunc()
{
	EventLoop   loop;
	{
		std::unique_lock<mutex>	lock(mutex_);
		loop_ = &loop;
		cond_.notify_one(); //线程已经完成，通知主线程
	}
	loop.loop();
}