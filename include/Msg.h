#pragma once
#include <stdint.h>
#include <memory.h>
#include <memory>
#include <string>
using namespace std;


//消息基类
class BaseMsg
{
public:
    enum TYPE                   //消息类型 
    {
        SERVICE = 1,
        SOCKET_ACCEPT = 2,
        SOCKET_RW = 3,
    };

    virtual ~BaseMsg(){};

    uint8_t type;
    char load[9999999]{};       //用于检测内存泄漏，仅用于调试
    //int c = sizeof(type);
};

//服务间消息
class ServiceMsg : public BaseMsg
{
public:
    uint32_t source;            //消息发送方
    std::shared_ptr<char> buff; //消息内容
    size_t size;                //消息内容大小
};

//有新连接
class SocketAcceptMsg : public BaseMsg
{
public:
    int listenFd;
    int clientFd;
};

//可读可写
class SocketRWMsg : public BaseMsg
{
public:
    int fd;
    bool isRead = false;
    bool isWrite = false;
};