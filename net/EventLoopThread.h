#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

#include "EventLoop.h"

namespace muduo
{
class EventLoop;

class EventLoopThread
{
public:
	EventLoopThread();
	~EventLoopThread();

	EventLoopThread(const EventLoopThread&) = delete;
	void operator=(const EventLoopThread&) = delete;
	
	EventLoop* startLoop();
private:
	void threadFunc();

	EventLoop*	loop_;
	bool			exiting_;
	std::thread	    thread_;
	std::mutex		mutex_;
	std::condition_variable cond_;
};

}