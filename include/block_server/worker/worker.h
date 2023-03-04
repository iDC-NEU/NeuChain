//
// Created by peng on 2021/1/15.
//

#pragma once

class WorkerInstance;
class TransactionExecutor;

class Worker {
public:
    explicit Worker(WorkerInstance* self) :self{self} { }
    virtual ~Worker();
    virtual void run() = 0;
    inline WorkerInstance* getInstance() { return self; }

protected:
    WorkerInstance* self;
};

