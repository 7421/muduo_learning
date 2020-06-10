#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include <assert.h>

namespace muduo
{
/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
class Buffer
{
public:
	static const size_t kCheapPrepend = 8;   //默认预留字节数
	static const size_t kInitialSize = 1024; //默认不包括预留字节数的缓冲区大小
	
	Buffer()
		: buffer_(kCheapPrepend + kInitialSize),
		  readerIndex_(kCheapPrepend),
		  writerIndex_(kCheapPrepend)
	{
		assert(readableBytes() == 0);
		assert(writableBytes() == kInitialSize);
		assert(prependableBytes() == kCheapPrepend);
	}
	/*
		支持拷贝构造函数与赋值构造函数
	*/

	void swap(Buffer& rhs)
	{
		buffer_.swap(rhs.buffer_);
		std::swap(readerIndex_, rhs.readerIndex_);
		std::swap(writerIndex_, rhs.writerIndex_);
	}
	//读缓冲区
	size_t readableBytes() const
	{
		return writerIndex_ - readerIndex_;
	}
	//写缓冲区
	size_t writableBytes() const
	{
		return buffer_.size() - writerIndex_;
	}
	//头部预留空间
	size_t prependableBytes() const
	{
		return readerIndex_;
	}
	//返回数据起始位置
	const char* peek() const
	{
		return begin() + readerIndex_;
	}
	//调整readerIndex指针位置，后移len
	void retrieve(size_t len)
	{
		assert(len <= readableBytes());
		readerIndex_ += len;
	}
	void retrieveUntil(const char* end)
	{
		assert(peek() <= end);
		assert(end <= beginWrite());
		retrieve(end - peek());
	}
	//初始化readerIndex_/writerIndex_位置，通常在用户将数据全部读出之后执行
	void retrieveAll()
	{
		readerIndex_ = kCheapPrepend;
		writerIndex_ = kCheapPrepend;
	}
	//从缓冲区中取出所有数据
	std::string retrieveAsString()
	{
		std::string str(peek(), readableBytes());
		retrieveAll();
		return str;
	}
	//栈空间数据追加到缓冲区末尾
	void append(const std::string& str)
	{
		append(str.data(),str.length());
	}
	void append(const void* data, size_t len)
	{
		append(static_cast<const char*>(data), len);
	}
	void append(const char* data, size_t len)
	{
		ensureWritableBytes(len);
		std::copy(data, data + len, beginWrite());
		hasWritten(len);
	}

	//确保缓冲区有len个大小的空间，不够就申请
	void ensureWritableBytes(size_t len)
	{
		if (writableBytes() < len)
		{
			makeSpace(len);
		}
		assert(writableBytes() >= len);
	}
	//返回写入开始点（即数据结束点）
	char* beginWrite()
	{
		return begin() + writerIndex_;
	}
	//移动数据结束点
	void hasWritten(size_t len)
	{
		writerIndex_ += len;
	}
	//将len个大小数据追加到可读数据头部
	void prepend(const void* data, size_t len)
	{
		assert(len <= prependableBytes());
		readerIndex_ -= len;
		const char* d = static_cast<const char*>(data);
		std::copy(d, d + len, begin() + readerIndex_);
	}
	//收缩
	void shrink(size_t reserve)
	{
		std::vector<char> buf(kCheapPrepend + readableBytes() + reserve);
		std::copy(peek(), peek() + readableBytes(), buf.begin() + kCheapPrepend);
		buf.swap(buffer_);
	}
	//直接读数据到buffer中
	ssize_t readFd(int fd, int* savedErrno);
private:
	char* begin()
	{
		return &*buffer_.begin();
	}

	const char* begin() const
	{
		return &*buffer_.begin();
	}
	/*
		多次从缓冲区独数据后，readerIndex会后移，导致预留变大
		在增大空间之前，先判断调整预留空间的大小后能否容纳要求的数据
		如果可以，则将预留空间缩小为8字节（默认的预留空间大小)
		如果不可以，那么就只能增加空间
	*/
	void makeSpace(size_t len)
	{
		if (writableBytes() + prependableBytes() < len + kCheapPrepend)
		{
			buffer_.resize(writerIndex_ + len);
		}
		else
		{
			//move readable data to the front, make space inside buffer
			assert(kCheapPrepend < readerIndex_);
			size_t readable = readableBytes();
			std::copy(begin() + readerIndex_,
				begin() + writerIndex_,
				begin() + kCheapPrepend);
			readerIndex_ = kCheapPrepend;
			writerIndex_ = readerIndex_ + readable;
			assert(readable == readableBytes());
		}
	}

    std::vector<char> buffer_; //缓冲区
    size_t readerIndex_;	   //数据起始点	
    size_t writerIndex_;	   //数据结束点
};
}