#pragma once
#include <thread>
using namespace std;

class Sunnet;
class Worker
{
public:
    int id;             //编号
    int eachNum;        //每次处理多少条消息
    void operator()();  //线程函数
};