#include "./net/EventLoop.h"
#include <iostream>
#include <unistd.h>
#include <thread>

void threadFunc(void)
{
	std::cout << "threadFunc(): pid = " << getpid()
		<< ", tid = " << std::this_thread::get_id() << std::endl;
	muduo::EventLoop loop;
	loop.loop();
}

int main()
{
	std::cout << "threadFunc(): pid = " << getpid()
		<< ", tid = " << std::this_thread::get_id() << std::endl;
	muduo::EventLoop  loop;
	std::thread	t(threadFunc);

	loop.loop();
	t.join();

	getchar();
}