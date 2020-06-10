#include <iostream>
#include <thread>
#include <functional>
#include <unistd.h>

#include "./net/EventLoop.h"
#include "./base/Timestamp.h"

int cnt = 0;
muduo::EventLoop* g_loop;

void printTid()
{
	std::cout << "pid = " << getpid() << ", tid = " << std::this_thread::get_id() << std::endl;
	std::cout << "now " << Timestamp::now().toString() << std::endl;
}

void print(const char* msg)
{
	std::cout << "ms " << Timestamp::now().toString() << msg << std::endl;
	if (++cnt == 20)
		g_loop->quit();
}

int main()
{
	printTid();
	muduo::EventLoop loop;

	g_loop = &loop;
	std::cout << "main" << Timestamp::now().toString() << std::endl;
	loop.runAfter(1, bind(print, "once1"));
	loop.runAfter(1.5, bind(print, "once1.5"));
	loop.runEvery(2, bind(print, "once2"));

	loop.loop();
	cout << "main loop exits" << std::endl;
	getchar();
}