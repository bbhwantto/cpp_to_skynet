#include <iostream>
#include <unistd.h>
#include "Worker.h"
#include "Service.h"
#include "Sunnet.h"
using namespace std;

//线程函数
void Worker::operator()()
{
    while(true)
    {
        shared_ptr<Service> srv = Sunnet::inst->PopGlobalQueue();
        if (!srv)
        {
            Sunnet::inst->WorkerWait();
        }
        else
        {
            srv->ProcessMsgs(eachNum);
            Sunnet::inst->CheckAndPutGlobal(srv);
        }
    }
}