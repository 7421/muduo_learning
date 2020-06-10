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
	static const size_t kCheapPrepend = 8;   //Ĭ��Ԥ���ֽ���
	static const size_t kInitialSize = 1024; //Ĭ�ϲ�����Ԥ���ֽ����Ļ�������С
	
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
		֧�ֿ������캯���븳ֵ���캯��
	*/

	void swap(Buffer& rhs)
	{
		buffer_.swap(rhs.buffer_);
		std::swap(readerIndex_, rhs.readerIndex_);
		std::swap(writerIndex_, rhs.writerIndex_);
	}
	//��������
	size_t readableBytes() const
	{
		return writerIndex_ - readerIndex_;
	}
	//д������
	size_t writableBytes() const
	{
		return buffer_.size() - writerIndex_;
	}
	//ͷ��Ԥ���ռ�
	size_t prependableBytes() const
	{
		return readerIndex_;
	}
	//����������ʼλ��
	const char* peek() const
	{
		return begin() + readerIndex_;
	}
	//����readerIndexָ��λ�ã�����len
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
	//��ʼ��readerIndex_/writerIndex_λ�ã�ͨ�����û�������ȫ������֮��ִ��
	void retrieveAll()
	{
		readerIndex_ = kCheapPrepend;
		writerIndex_ = kCheapPrepend;
	}
	//�ӻ�������ȡ����������
	std::string retrieveAsString()
	{
		std::string str(peek(), readableBytes());
		retrieveAll();
		return str;
	}
	//ջ�ռ�����׷�ӵ�������ĩβ
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

	//ȷ����������len����С�Ŀռ䣬����������
	void ensureWritableBytes(size_t len)
	{
		if (writableBytes() < len)
		{
			makeSpace(len);
		}
		assert(writableBytes() >= len);
	}
	//����д�뿪ʼ�㣨�����ݽ����㣩
	char* beginWrite()
	{
		return begin() + writerIndex_;
	}
	//�ƶ����ݽ�����
	void hasWritten(size_t len)
	{
		writerIndex_ += len;
	}
	//��len����С����׷�ӵ��ɶ�����ͷ��
	void prepend(const void* data, size_t len)
	{
		assert(len <= prependableBytes());
		readerIndex_ -= len;
		const char* d = static_cast<const char*>(data);
		std::copy(d, d + len, begin() + readerIndex_);
	}
	//����
	void shrink(size_t reserve)
	{
		std::vector<char> buf(kCheapPrepend + readableBytes() + reserve);
		std::copy(peek(), peek() + readableBytes(), buf.begin() + kCheapPrepend);
		buf.swap(buffer_);
	}
	//ֱ�Ӷ����ݵ�buffer��
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
		��δӻ����������ݺ�readerIndex����ƣ�����Ԥ�����
		������ռ�֮ǰ�����жϵ���Ԥ���ռ�Ĵ�С���ܷ�����Ҫ�������
		������ԣ���Ԥ���ռ���СΪ8�ֽڣ�Ĭ�ϵ�Ԥ���ռ��С)
		��������ԣ���ô��ֻ�����ӿռ�
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

    std::vector<char> buffer_; //������
    size_t readerIndex_;	   //������ʼ��	
    size_t writerIndex_;	   //���ݽ�����
};
}