#include "Service.h"
#include "Sunnet.h"
#include <iostream>
#include <unistd.h>
#include <string.h>
#include "LuaAPI.h"

Service::Service()
{
    //初始化锁
    pthread_spin_init(&queueLock, PTHREAD_PROCESS_PRIVATE);    
    pthread_spin_init(&inGlobalLock, PTHREAD_PROCESS_PRIVATE);
}

Service::~Service()
{
    pthread_spin_destroy(&queueLock);
    pthread_spin_destroy(&inGlobalLock);
}

void Service::PushMsg(shared_ptr<BaseMsg> msg)
{
    pthread_spin_lock(&queueLock);
    {
        msgQueue.push(msg);
    }
    pthread_spin_unlock(&queueLock);
}

shared_ptr<BaseMsg> Service::PopMsg()
{
    shared_ptr<BaseMsg> msg = NULL;
    
    //取出一条消息
    pthread_spin_lock(&queueLock);
    {
        if (!msgQueue.empty())
        {
            msg = msgQueue.front();
            msgQueue.pop();
        }
    }
    pthread_spin_unlock(&queueLock);
    return msg;
}

void Service::OnInit()
{
    cout << "[ " << id <<" ] OnInit" << endl;
    //Sunnet::inst->Listen(8002, id);
    //新建lua虚拟机
    luaState = luaL_newstate();
    luaL_openlibs(luaState);
    //注册Sunnet系统API
    LuaAPI::Register(luaState);
    //执行lua文件
    string filename = "../service/" + *type + "/init.lua";
    int isok = luaL_dofile(luaState, filename.data());
    if(isok == 1)
    {
        cout << "run lua fail: " << lua_tostring(luaState, -1) << endl;
    }
    //调用lua函数
    lua_getglobal(luaState, "OnInit");
    lua_pushinteger(luaState, id);
    isok = lua_pcall(luaState, 1, 0, 0);
    if (isok != 0)
    {
        cout << "call lua OnInit fail " << lua_tostring(luaState, -1) << endl;

    }
}

void Service::OnMsg(shared_ptr<BaseMsg> msg)
{
    
    if(msg->type == BaseMsg::TYPE::SERVICE)
    {cout << "1" <<endl;
        auto m = dynamic_pointer_cast<ServiceMsg>(msg);
        OnServiceMsg(m);
    }
    else if(msg->type == BaseMsg::TYPE::SOCKET_ACCEPT)
    {cout << "2" <<endl;
        auto m = dynamic_pointer_cast<SocketAcceptMsg>(msg);
        OnAcceptMsg(m);
    }
    else if(msg->type == BaseMsg::TYPE::SOCKET_RW)
    {cout << "3" <<endl;
        auto m = dynamic_pointer_cast<SocketRWMsg>(msg);
        OnRWMsg(m);
    }
    
}

void Service::OnExit()
{
    cout << "[ " << id <<" ] OnExit" << endl;
    
    //调用lua函数
    lua_getglobal(luaState, "OnExit");
    int isok = lua_pcall(luaState, 0, 0, 0);
    if (isok != 0)
    {
        cout << "call lua OnInit fail " << lua_tostring(luaState, -1) << endl;
    }
    //关闭lua虚拟机
    lua_close(luaState);
}

bool Service::ProcessMsg()
{
    shared_ptr<BaseMsg> msg = PopMsg();
    if (msg)
    {
        OnMsg(msg);
        return true;
    }
    else
    {
        return false;
    }
}

void Service::ProcessMsgs(int max)
{
    for (int i = 0; i < max; i++)
    {
        bool succ = ProcessMsg();
        if(!succ)
        {
            break;
        }
    }
}

void Service::SetInGlobal(bool isIn)
{
    pthread_spin_lock(&inGlobalLock);
    {
        inGlobal = isIn;
    }
    pthread_spin_unlock(&inGlobalLock);
}

void Service::OnServiceMsg(shared_ptr<ServiceMsg> msg)
{
    cout << "OnServiceMsg" << endl;
    //调用lua函数
    lua_getglobal(luaState, "OnServiceMsg");
    lua_pushinteger(luaState, msg->source);
    lua_pushlstring(luaState, msg->buff.get(), msg->size);
    int isok = lua_pcall(luaState, 2, 0, 0);
    if(isok != 0)
    {
        cout << "call lua OnServiceMsg fail" << lua_tostring(luaState, -1) << endl;
    }
}

void Service::OnAcceptMsg(shared_ptr<SocketAcceptMsg> msg)
{
    cout << "OnAcceptMsg" << msg->clientFd << endl;
}

void Service::OnRWMsg(shared_ptr<SocketRWMsg> msg)
{
    int fd = msg->fd;
    //可读
    if(msg->isRead)
    {
        const int BUFFSIZE = 512;
        char buff[BUFFSIZE];
        int len = 0;
        do
        {
            len = read(fd, &buff, BUFFSIZE);
            if(len > 0)
            {
                OnSocketData(fd, buff, len);
            }
        } while (len == BUFFSIZE);
        
        if(len <= 0 && errno != EAGAIN)
        {
            if(Sunnet::inst->GetConn(fd))
            {
                OnSocketClose(fd);
                Sunnet::inst->CloseConn(fd);
            }
        }
    }

    //可写
    if(msg->isWrite)
    {
        if(Sunnet::inst->GetConn(fd))
        {
            OnSocketWritable(fd);
        }
    }
}

//收到客户端数据
void Service::OnSocketData(int fd, const char* buff, int len)
{
    //调用lua函数
    lua_getglobal(luaState, "OnSocketData");
    lua_pushinteger(luaState, fd);
    lua_pushlstring(luaState, buff, len);
    int isok = lua_pcall(luaState, 2, 0, 0);
    if(isok != 0)
    {
        cout << "call lua OnSocketData fail " << lua_tostring(luaState, -1) << endl;
    }


    // cout << "OnSocketData" << fd << " buff: " << buff <<endl;
    // char writeBuff[3] = {'l', 'p', 'y'};
    // write(fd, &writeBuff, 3);
    // //等待15s，继续发送
    // usleep(15000000);   //15s
    // char writeBuff2[3] = {'n', 'e', 't'};
    // int r = write(fd, &writeBuff2, 3);
    // cout << "write2 r:" << r << " " << strerror(errno) << endl;
    // //等待1s，继续发送
    // usleep(1000000);    //1s
    // char writeBuff3[2] = {'n', 'o'};
    // r = write(fd, &writeBuff3, 2);
    // cout << "write3 r: " << r << " " << strerror(errno) << endl;
}

//套接字可写
void Service::OnSocketWritable(int fd)
{
    cout << "OnSocketWritable: " << fd << endl;
}

//关闭连接前
void Service::OnSocketClose(int fd)
{
    cout << "OnSocketClose: " << fd << endl; 
}