#pragma once

#include <stdio.h>
#include <string>
#include <list>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>

#define LOG_API

enum LOG_LEVEL
{
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,    //����ҵ�����
    LOG_LEVEL_SYSERROR, //���ڼ�����ܱ����Ĵ���
    LOG_LEVEL_FATAL,    //FATAL �������־�����ڳ��������־���˳�
    LOG_LEVEL_CRITICAL  //CRITICAL ��־������־������ƣ��������
};

//TODO: �����Ӽ�������
//ע�⣺�����ӡ����־��Ϣ�������ģ����ʽ���ַ���Ҫ��_T()�����������
//e.g. LOGI(_T("GroupID=%u, GroupName=%s, GroupName=%s."), lpGroupInfo->m_nGroupCode, lpGroupInfo->m_strAccount.c_str(), lpGroupInfo->m_strName.c_str());
#define LOGT(...)    CAsyncLog::output(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOGD(...)    CAsyncLog::output(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOGI(...)    CAsyncLog::output(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOGW(...)    CAsyncLog::output(LOG_LEVEL_WARNING, __FILE__, __LINE__,__VA_ARGS__)
#define LOGE(...)    CAsyncLog::output(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOGSYSE(...) CAsyncLog::output(LOG_LEVEL_SYSERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOGF(...)    CAsyncLog::output(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)        //Ϊ����FATAL�������־������crash���򣬲�ȡͬ��д��־�ķ���
#define LOGC(...)    CAsyncLog::output(LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __VA_ARGS__)     //�ؼ���Ϣ��������־�����������

//����������ݰ��Ķ����Ƹ�ʽ
#define LOG_DEBUG_BIN(buf, buflength) CAsyncLog::outputBinary(buf, buflength)

class LOG_API CAsyncLog
{
public:
    static bool init(const char* pszLogFileName = nullptr, bool bTruncateLongLine = false, int64_t nRollSize = 10 * 1024 * 1024);
    static void uninit();

    static void setLevel(LOG_LEVEL nLevel);
    static bool isRunning();

    //����߳�ID�ź����ں���ǩ�����к�
    static bool output(long nLevel, const char* pszFileName, int nLineNo, const char* pszFmt, ...);
private:
    CAsyncLog() = delete;
    ~CAsyncLog() = delete;

    CAsyncLog(const CAsyncLog& rhs) = delete;
    CAsyncLog& operator=(const CAsyncLog& rhs) = delete;
    //������־ǰ׺ [����] + [ʱ��] + [��ǰ�߳�]
    static void makeLinePrefix(long nLevel, std::string& strPrefix);
    static void getTime(char* pszTime, int nTimeStrLength);
    static bool createNewFile(const char* pszLogFileName);
    static bool writeToFile(const std::string& data);
    //�ó�����������
    static void crash();

    static void writeThreadProc();

private:
    static bool                         m_bToFile;              //��־д���ļ�����д������̨
    static FILE*                        m_hLogFile;   
    static std::string                  m_strFileName;          //��־�ļ���
    static std::string                  m_strFileNamePID;       //�ļ����еĽ���ID
    static bool                         m_bTruncateLongLog;     //����־�Ƿ�ض�
    static LOG_LEVEL                    m_nCurrentLevel;        //��ǰ��־����
    static int64_t                      m_nFileRollSize;        //������־�ļ�������ֽ���
    static int64_t                      m_nCurrentWrittenSize;  //�Ѿ�д����ֽ���Ŀ
    static std::list<std::string>       m_listLinesToWrite;     //��д�����־
    static std::unique_ptr<std::thread> m_spWriteThread; 
    static std::mutex                   m_mutexWrite;
    static std::condition_variable      m_cvWrite;
    static bool                         m_bExit;                //�˳���ʶ
    static bool                         m_bRunning;             //���б�ʶ

};