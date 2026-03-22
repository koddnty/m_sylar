#pragma once
#include <shared_mutex>
#include <memory>
#include <list>
#include "fiber.h"
#include "basic/config.h"

namespace m_sylar
{
#define M_SYLAR_SINGLE_GET_TASK_NUM 15          // 默认单次任务取用大小
#define M_SYLAR_MAX_LOOP_IDX 999999999          // 索引归零阈值

class TaskPoolNode
{
public:
    using ptr = std::shared_ptr<TaskPoolNode>;

    TaskPoolNode();
    ~TaskPoolNode();

    bool addTask(TaskCoro20&& tasks);
    bool addTask(std::list<TaskCoro20>&& tasks);
    bool addTask(std::list<TaskCoro20>& tasks);
    int getTask(std::list<TaskCoro20>& tasks, int num = M_SYLAR_SINGLE_GET_TASK_NUM);


private:
    std::shared_mutex m_mutex;
    std::list<TaskCoro20> m_tasks;
};





/** 
    @brief TaskLoop管理者，负责从自己管理的所有TaskLoop中添加、获取任务等
*/
class TaskPool
{
public:
    using ptr = std::shared_ptr<TaskPool>;

    TaskPool(int nodeNum);
    ~TaskPool();


    void taskAlloc(TaskCoro20&& task);                      // 任务分配器
    int taskGet(std::list<TaskCoro20>& tasks);             // 任务获取器

    int getTaskCount() const {return m_task_count; }

private: 
    std::shared_mutex m_mutex;          // 状态更新锁
    std::vector<TaskPoolNode::ptr> m_nodes; 
    std::atomic<uint32_t> m_loop_idx = 0;
    std::atomic<uint32_t> m_alloc_idx = 0;
    std::atomic<uint32_t> m_task_count = 0;
    int m_node_num = 0;
};

}