#pragma once

#include<stdio.h>
#include <sys/types.h>

namespace muduo
{
class Timer;

class TimerId
{
public:
	TimerId(Timer* timer = NULL ,int64_t seq = 0)
		: timer_(timer),
		  sequence_(seq)
	{
	}
	//友元类的成员函数可以访问此类包括非共有成员在内的所有成员
	friend class TimerQueue;

private:
	Timer* timer_;
	int64_t sequence_;
};

}