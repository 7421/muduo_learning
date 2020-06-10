#include "Timer.h"

using namespace muduo;

atomic_llong  Timer::s_numCreated_;

//���ö�ʱ���������Զ�ʱ�������������ö�ʱʱ��
//������Ϊ����Чʱ���
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