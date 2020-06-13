#include <iostream>
#include <thread>
#include <unistd.h>
#include <functional>

#include "./net/EventLoop.h"
#include "./net/EventLoopThread.h"

muduo::EventLoop* g_loop;
int g_flag = 0;

void print(const char* msg)
{
    std::cout << msg << "  pid =  "
        << getpid() << ", tid = "
        << std::this_thread::get_id() << std::endl;
}

int main()
{
    print("main()");
    muduo::EventLoopThread loopThread;
    muduo::EventLoop* loop = loopThread.startLoop();
    loop->runInLoop(std::bind(print, "runInThread: "));
    sleep(1);
    loop->runAfter(2, std::bind(print, "runInThread: "));
    sleep(3);
    loop->quit();
    getchar();
}