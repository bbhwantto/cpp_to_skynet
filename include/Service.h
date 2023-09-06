#pragma once
#include <queue>
#include <thread>
#include "Msg.h"

extern "C" {
 #include "lua.h"
 #include "lauxlib.h"
 #include "lualib.h"
} 

using namespace std;

class Service
{
public:
    Service();        
    ~Service();

    //回调函数(编写服务逻辑)
    void OnInit();                              //创建服务后触发
    void OnMsg(shared_ptr<BaseMsg> msg);        //收到消息时触发
    void OnExit();                              //退出服务时触发

    void PushMsg(shared_ptr<BaseMsg> msg);      //插入消息
    bool ProcessMsg();                          //处理一条消息，返回值代表是否处理
    void ProcessMsgs(int max);                  //处理N条消息，返回值代表是否处理

    void SetInGlobal(bool isIn);                //线程安全的设置inGlobal

    void OnServiceMsg(shared_ptr<ServiceMsg> msg);
    void OnAcceptMsg(shared_ptr<SocketAcceptMsg> msg);
    void OnRWMsg(shared_ptr<SocketRWMsg> msg);
    void OnSocketData(int fd, const char* buff, int len);
    void OnSocketWritable(int fd);
    void OnSocketClose(int fd);


private:
    shared_ptr<BaseMsg> PopMsg();               //取出一条消息  
    
    lua_State* luaState;

public:
    uint32_t id;                                //唯一id
    shared_ptr<string> type;                    //类型
    bool isExiting = false;                     //是否正在退出
    queue<shared_ptr<BaseMsg>> msgQueue;        //消息列表
    pthread_spinlock_t queueLock;               //锁
    bool  inGlobal = false;                     //标记是否在全局队列，true代表在
    pthread_spinlock_t inGlobalLock;            //inGlobal的锁
};