#include "EventLoopThread.h"
#include "EventLoop.h"
#include <functional>
#include <thread>
#include <mutex>

using namespace muduo;

EventLoopThread::EventLoopThread()
	: loop_(NULL),		//�����̴߳�����EventLoop*
	  exiting_(false),  
	  thread_(),		//һ���յ�thread����,���������κ��߳�
	  mutex_(),		    //std::mutex		mutex_; ������
	  cond_()			//std::condition_variable ��������
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
		//����ϵͳ�жϣ���������������ٻ��ѣ���Ҫ�ٴμ�⣬loop�Ƿ񴴽����
		while (loop_ == NULL)
		{
			cond_.wait(lock);   //���������������ͷ������ȴ����������ٴλ����
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
		cond_.notify_one(); //�߳��Ѿ���ɣ�֪ͨ���߳�
	}
	loop.loop();
}