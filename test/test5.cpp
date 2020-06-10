#include <iostream>
#include <thread>
#include <unistd.h>

#include "./net/EventLoop.h"

muduo::EventLoop* g_loop;
int g_flag = 0;

void print(const char* msg)
{
    std::cout << msg << "  pid =  "
        << getpid() << ", tid = "
        << std::this_thread::get_id() << " g_flag = "
        << g_flag << std::endl;
}

void run4()
{
    print("run4");
    g_loop->quit();
}

void run3()
{
    print("run3");
    g_loop->runAfter(3, run4);
    g_flag = 3;
}

void run2()
{
    print("run2");
    g_loop->queueInLoop(run3);
}

void run1()
{
    g_flag = 1;
    print("run1");
    g_loop->runInLoop(run2);
    g_flag = 2;
}
int main()
{
    print("");
    muduo::EventLoop loop;

    g_loop = &loop;

    loop.runAfter(2, run1);
    loop.loop();
    print("main()");
    getchar();
}