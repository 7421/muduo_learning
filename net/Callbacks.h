#pragma once

#include <functional>
#include <memory>
#include "Buffer.h"
#include "../base/Timestamp.h"
namespace muduo
{
//所有用户可见 
class TcpConnection;

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
typedef std::function<void()> TimerCallback;
typedef std::function<void(const TcpConnectionPtr&)> ConnectionCallback;
typedef std::function<void(const TcpConnectionPtr&, Buffer* data, Timestamp)> MessageCallback;
typedef std::function<void(const TcpConnectionPtr&)> WriteCompleteCallback;
typedef std::function<void(const TcpConnectionPtr&)> CloseCallback;
}