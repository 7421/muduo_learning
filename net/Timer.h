#pragma once

#include "../base/Timestamp.h"
#include "Callbacks.h"
#include <atomic>

namespace muduo
{
class Timer
{
public:
	Timer(const TimerCallback& cb, Timestamp when, double interval)
		: callback_(cb),
		expiration_(when),
		interval_(interval),
		repeat_(interval > 0.0),
		sequence_(++s_numCreated_ )
	{ }

	//调用定时回调函数
	void run() const
	{
		callback_();
	}

	//返回定时时间
	Timestamp	expiration() const { return expiration_; }
	//回答用户当前定时器是否周期性运行
	bool repeat() const { return repeat_; }
	int64_t sequence() const { return sequence_; }
	void restart(Timestamp now);

private:
	const TimerCallback callback_;	//定时回调函数
	Timestamp    expiration_;		//定时时间
	const double interval_;			//周期性定时时间
	const bool   repeat_;			//是否周期性定时
	const int64_t sequence_;

	static atomic_llong   s_numCreated_;
};
}

