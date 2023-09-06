#pragma once
#include <vector>
#include "Worker.h"
#include "Service.h"
#include "SocketWorker.h"
#include "Conn.h"
#include <unordered_map>

class Worker;

class Sunnet
{
public:
    static Sunnet* inst;                                    //单例
public:
    Sunnet();                                               //构造函数
    void Start();                                           //初始化并开始
    void Wait();                                            //等待运行
    void StartWorker();                                     //开启工作线程
public:
    uint32_t NewService(shared_ptr<string> type);           //增删服务
    void KillService(uint32_t id);                          //仅限服务自己调用
    void Send(uint32_t toId, shared_ptr<BaseMsg> msg);      //发送消息
    shared_ptr<Service> PopGlobalQueue();                   //弹出队列操作
    void PushGlobalQueue(shared_ptr<Service> srv);          //插入全局队列
    void CheckAndPutGlobal(shared_ptr<Service> srv);
    shared_ptr<BaseMsg> MakeMsg(uint32_t source, char* buff, int len);  //测试用
    void CheckAndWeakUp();                                  //唤醒工作线程
    void WorkerWait();                                      //让工作线程等待
    void StartSocket();                                     //开启socket线程

    //增删查Conn
    int AddConn(int fd, uint32_t id, Conn::TYPE type);
    shared_ptr<Conn> GetConn(int fd);
    bool RemoveConn(int fd);

    //网络连接操作接口
    int Listen(uint32_t port, uint32_t serviceId);
    void CloseConn(uint32_t fd);

private:
    shared_ptr<Service> GetService(uint32_t id);            //获取服务

private:

    //工作线程
    int WORKER_NUM = 3;             //工作线程数
    vector<Worker* > workers;       //worker对象
    vector<thread* > workerThreads; //线程

    unordered_map<uint32_t, shared_ptr<Service>> services;  //服务列表
    uint32_t maxId = 0;                                     //最大ID
    pthread_rwlock_t servicesLock;                          //读写锁

    queue<shared_ptr<Service>> globalQueue;                 //全局队列
    int globalLen = 0;                                      //队列长度
    pthread_spinlock_t globalLock;                          //锁

    pthread_mutex_t sleepMtx;                               //休眠和唤醒的锁、条件变量
    pthread_cond_t sleepCond;        
    int sleepCount = 0;

    SocketWorker* socketWorker;                             //socket线程
    thread* socketThread;

    unordered_map<uint32_t, shared_ptr<Conn>> conns;
    pthread_rwlock_t connsLock;

};
