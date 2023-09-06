#include <iostream>
#include <assert.h>
#include "Sunnet.h"
#include "Msg.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
using namespace std;

//单例
Sunnet* Sunnet::inst;
Sunnet::Sunnet()
{
    inst = this;
    pthread_cond_init(&sleepCond, NULL);
    pthread_mutex_init(&sleepMtx, NULL);
}

//开启系统
void Sunnet::Start()
{
    cout << "Hello Sunnet\n";
    signal(SIGPIPE, SIG_IGN);
    //初始化锁
    pthread_rwlock_init(&servicesLock, NULL);
    pthread_spin_init(&globalLock, PTHREAD_PROCESS_PRIVATE);
    //开启工作线程
    StartWorker();
    //开启Socket线程
    StartSocket();

    assert(pthread_rwlock_init(&connsLock, NULL) == 0);
}

//开动worker线程
void Sunnet::StartWorker()
{
    for (int i = 0; i < WORKER_NUM; i++)
    {
        cout <<"start worker thread: " << i << endl;
        //创建线程对象
        Worker* worker = new Worker();
        worker->id = i;
        worker->eachNum = 2 << i;
        //创建线程
        thread* wt = new thread(*worker);
        //添加到到数组
        workers.push_back(worker);
        workerThreads.push_back(wt);
    }
}

//等待
void Sunnet::Wait()
{
    if (workerThreads[0])
    {
        workerThreads[0]->join();
    }
}

uint32_t Sunnet::NewService(shared_ptr<string> type)
{
    auto srv = make_shared<Service>();
    srv->type = type;
    pthread_rwlock_wrlock(&servicesLock);
    {
        srv->id = maxId;
        maxId++;
        services.emplace(srv->id, srv);
    }
    pthread_rwlock_unlock(&servicesLock);
    srv->OnInit();
    return srv->id;
}

shared_ptr<Service> Sunnet::GetService(uint32_t id)
{
    shared_ptr<Service> srv = NULL;
    pthread_rwlock_rdlock(&servicesLock);
    {
        auto iter = services.find(id);
        if(iter != services.end())
        {
            srv = iter->second;
        }
    }
    pthread_rwlock_unlock(&servicesLock);
    return srv;
}

void Sunnet::KillService(uint32_t id)
{
    shared_ptr<Service> srv = GetService(id);
    if(!srv)
    {
        return ;
    }

    //退出前
    srv->OnExit();
    srv->isExiting = true;
    //删除列表
    pthread_rwlock_wrlock(&servicesLock);
    {
        services.erase(id);
    }
    pthread_rwlock_unlock(&servicesLock);
}

shared_ptr<Service> Sunnet::PopGlobalQueue()
{
    shared_ptr<Service> srv = NULL;
    pthread_spin_lock(&globalLock);
    {
        if (!globalQueue.empty())
        {
            srv = globalQueue.front();
            globalQueue.pop();
            globalLen--;
        }
    }
    pthread_spin_unlock(&globalLock);
    return srv;
}

void Sunnet::PushGlobalQueue(shared_ptr<Service> srv)
{
    pthread_spin_lock(&globalLock);
    {
        globalQueue.push(srv);
        globalLen++;
    }
    pthread_spin_unlock(&globalLock);
}

void Sunnet::Send(uint32_t toId, shared_ptr<BaseMsg> msg)
{
    shared_ptr<Service> toSrv = GetService(toId);
    if(!toSrv)
    {
        cout << "Send fail, toSrv not exist toId: " <<toId << endl;
        return ;
    }
    //插入目标服务的消息队列
    toSrv->PushMsg(msg);
    //检查并放入全局队列
    bool hasPush = false;
    pthread_spin_lock(&toSrv->inGlobalLock);
    {
        if(!toSrv->inGlobal)
        {
            PushGlobalQueue(toSrv);
            toSrv->inGlobal = true;
            hasPush = true;
        }
    }
    pthread_spin_unlock(&toSrv->inGlobalLock);

    //唤起进程
    if(hasPush)
    {
        CheckAndWeakUp();
    }
}

void Sunnet::CheckAndPutGlobal(shared_ptr<Service> srv)
{
    if(srv->isExiting)
    {
        return;
    }

    pthread_spin_lock(&srv->queueLock);
    {
        if(!srv->msgQueue.empty())
        {
            Sunnet::inst->PushGlobalQueue(srv);
        }
        else
        {
            srv->SetInGlobal(false);
        }
    }
    pthread_spin_unlock(&srv->queueLock);
}

shared_ptr<BaseMsg> Sunnet::MakeMsg(uint32_t source, char* buff, int len)
{
    auto msg = make_shared<ServiceMsg>();
    msg->type = BaseMsg::TYPE::SERVICE;
    msg->source = source;
    msg->buff = shared_ptr<char>(buff);
    msg->size = len;
    return msg;
}

void Sunnet::CheckAndWeakUp()                       
{
    if(sleepCount == 0)
    {
        return ;
    }
    if(WORKER_NUM - sleepCount <= globalLen)
    {
        cout << "weak up" << endl;
        pthread_cond_signal(&sleepCond);
    }
}

void Sunnet::WorkerWait()
{
    pthread_mutex_lock(&sleepMtx);
    sleepCount++;
    pthread_cond_wait(&sleepCond, &sleepMtx);
    sleepCount--;
    pthread_mutex_unlock(&sleepMtx);
}

void Sunnet::StartSocket()
{
    socketWorker = new SocketWorker();
    socketWorker->Init();
    socketThread = new thread(*socketWorker);
}

int Sunnet::AddConn(int fd, uint32_t id, Conn::TYPE type)
{
    auto conn = make_shared<Conn>();
    conn->fd = fd;
    conn->serviceId = id;
    conn->type = type;
    cout << "AddConn fd: " << fd <<endl;
    pthread_rwlock_wrlock(&connsLock);
    {
        conns.emplace(fd, conn);
    }
    pthread_rwlock_unlock(&connsLock);
    return fd;
}

shared_ptr<Conn> Sunnet::GetConn(int fd)
{
    shared_ptr<Conn> conn = NULL;
    pthread_rwlock_rdlock(&connsLock);
    {
        auto iter = conns.find(fd);
        if(iter != conns.end())
        {
            conn = iter->second;
        }
    }
    pthread_rwlock_unlock(&connsLock);
    return conn;
}

bool Sunnet::RemoveConn(int fd)
{
    int result;
    pthread_rwlock_wrlock(&connsLock);
    {
        result = conns.erase(fd);
    }
    pthread_rwlock_unlock(&connsLock);
    return result == 1;
}

int Sunnet::Listen(uint32_t port, uint32_t serviceId)
{
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    //设置端口复用
    if(setsockopt(listenFd , SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) 
    {
        close(listenFd);
        return -1;
    }
    if(listenFd <= 0)
    {
        cout << "listen error, listenFd <= 0" << endl;
        return -1;
    }

    fcntl(listenFd, F_SETFL, O_NONBLOCK);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int r = bind(listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if (r == -1)
    {
        cout << "listen error, bind fail" << endl;
        return -1;
    }

    r = listen(listenFd, 64);
    if(r < 0)
    {
        return -1;
    }

    AddConn(listenFd, serviceId, Conn::TYPE::LISTEN);
    socketWorker->AddEvent(listenFd);

    return listenFd;
}

void Sunnet::CloseConn(uint32_t fd)
{
    bool succ = RemoveConn(fd);
    close(fd);
    if(succ)
    {
        socketWorker->RemoveEvent(fd);
    }
}