#include "./net/EventLoop.h"
#include "./net/Channel.h"

#include <iostream>
#include <sys/timerfd.h>
#include <unistd.h>
#include <memory.h>

muduo::EventLoop* g_loop;

void timeout()
{
	std::cout << "Timeout!\n" << std::endl;
	g_loop->quit();
}

int main()
{
	muduo::EventLoop loop;
	g_loop = &loop;

	/*
		函数创建一个定时器对象，同时返回一个与之关联的文件描述符。
		int timerfd_create(int clockid,int flags);
		CLOCK_MONOTONIC: 定时时间从此刻开始
	*/

	int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	muduo::Channel channel(&loop, timerfd);

	channel.setReadCallback(timeout);
	channel.enableReading();

	struct itimerspec howlong;

	memset(&howlong, 0, sizeof howlong);
	howlong.it_value.tv_sec = 5;
	/*
		#include <sys/timerfd.h>

		struct timespec {
			time_t tv_sec;                 Seconds
			long   tv_nsec;                Nanoseconds
		};

		struct itimerspec {
			struct timespec it_interval;   Interval for periodic timer （定时间隔周期）
			struct timespec it_value;      Initial expiration (第一次超时时间)
		};

		int timerfd_settime(int fd, int flags, const struct itimerspec* new_value, struct itimerspec* old_value);
		*/
	::timerfd_settime(timerfd, 0, &howlong, NULL);

	loop.loop();

	::close(timerfd);
	getchar();
}