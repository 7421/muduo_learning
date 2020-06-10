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
	//��Ԫ��ĳ�Ա�������Է��ʴ�������ǹ��г�Ա���ڵ����г�Ա
	friend class TimerQueue;

private:
	Timer* timer_;
	int64_t sequence_;
};

}