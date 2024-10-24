#ifndef _TASK_H_
#define _TASK_H_

#include "stdafx.h"

// 任务线程基类
class THREADPOOL_LIB CTask
{
public:
    // 构造
    CTask() {}

    // 析构
    virtual ~CTask() {}

    // 任务执行纯虚函数
    virtual void run() = 0;
};

#endif //!_TASK_H_