#include "taskPool.h"

namespace m_sylar
{

TaskPoolNode::TaskPoolNode()
{
}

TaskPoolNode::~TaskPoolNode()
{
}

bool TaskPoolNode::addTask(TaskCoro20&& tasks)
{
    std::unique_lock<std::shared_mutex> w_lock(m_mutex);
    m_tasks.push_back(std::move(tasks));
    return true;
}

bool TaskPoolNode::addTask(std::list<TaskCoro20>&& tasks)
{
    std::unique_lock<std::shared_mutex> w_lock(m_mutex);
    m_tasks.splice(m_tasks.end(), std::move(tasks));
    return true;
}

bool TaskPoolNode::addTask(std::list<TaskCoro20>& tasks)
{
    std::unique_lock<std::shared_mutex> w_lock(m_mutex);
    for(auto& it : tasks)
    {
        m_tasks.push_back(it.clone());
    }
    return true;
}

int TaskPoolNode::getTask(std::list<TaskCoro20>& tasks, int num)
{
    std::unique_lock<std::shared_mutex> w_lock(m_mutex);
    if(m_tasks.empty()) {return 0;}
    auto end = m_tasks.begin(); 
    int task_num = m_tasks.size();
    int rt = num;
    if(task_num < num) {end = m_tasks.end(); rt = task_num; }
    else {std::advance(end, num); }
    tasks.splice(tasks.end(), m_tasks, m_tasks.begin(), end);
    return rt;
}





TaskPool::TaskPool(int nodeNum)
    : m_node_num(nodeNum)
{
    m_nodes.resize(nodeNum);
    for(int i = 0; i < nodeNum; i++)
    {
        m_nodes[i] = std::make_shared<TaskPoolNode>();
    }
}

TaskPool::~TaskPool()
{
}

void TaskPool::taskAlloc(TaskCoro20&& task)
{
    m_nodes[m_alloc_idx++ % m_node_num]->addTask(std::move(task));
    m_task_count++;
}

int TaskPool::taskGet(std::list<TaskCoro20>& tasks)
{
    int rt = m_nodes[m_loop_idx++ % m_node_num]->getTask(tasks);
    m_task_count -= rt;
    std::string info = "task count is litter than 0, value = " + std::to_string(m_task_count);
    M_SYLAR_ASSERT2(m_task_count >= 0, info.c_str()); 

    return rt;
}



}