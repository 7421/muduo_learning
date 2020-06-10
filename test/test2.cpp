#include "./net/EventLoop.h"
#include <thread>

muduo::EventLoop* g_loop;

void threadFunc()
{
	g_loop->loop();
}

int main()
{
	muduo::EventLoop loop;
	g_loop = &loop;
	std::thread t(threadFunc);
	t.join();
}