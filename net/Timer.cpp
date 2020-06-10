#include "Timer.h"

using namespace muduo;

atomic_llong  Timer::s_numCreated_;

//若该定时器是周期性定时器，则重新设置定时时间
//否则设为：无效时间戳
void Timer::restart(Timestamp now)
{
	if (repeat_)
	{
		expiration_ = addTime(now, int64_t(interval_ * 1000000));
	}
	else
	{
		expiration_ = Timestamp::invalid();  
	}
}