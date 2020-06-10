#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <vector>
#include <functional>
#include <memory>

namespace muduo
{
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool
{
public:
	EventLoopThreadPool(EventLoop* baseLoop);
	~EventLoopThreadPool();

	EventLoopThreadPool(const EventLoopThreadPool&) = delete;
	void operator=(const EventLoopThreadPool&) = delete;

	void setThreadNum(int numThreads)
	{
		numThreads_ = numThreads;
	}
	void start();
	EventLoop* getNextLoop();

private:
	EventLoop* baseLoop_;
	bool	   started_;
	int	       numThreads_;
	int		   next_; //always in loop thread
	std::vector<std::shared_ptr<EventLoopThread>> threads_;
	std::vector<EventLoop*> loops_;
};

}