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

	//���ö�ʱ�ص�����
	void run() const
	{
		callback_();
	}

	//���ض�ʱʱ��
	Timestamp	expiration() const { return expiration_; }
	//�ش��û���ǰ��ʱ���Ƿ�����������
	bool repeat() const { return repeat_; }
	int64_t sequence() const { return sequence_; }
	void restart(Timestamp now);

private:
	const TimerCallback callback_;	//��ʱ�ص�����
	Timestamp    expiration_;		//��ʱʱ��
	const double interval_;			//�����Զ�ʱʱ��
	const bool   repeat_;			//�Ƿ������Զ�ʱ
	const int64_t sequence_;

	static atomic_llong   s_numCreated_;
};
}

