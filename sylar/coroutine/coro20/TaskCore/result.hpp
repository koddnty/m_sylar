#pragma once 
#include"task.hpp"

namespace m_sylar
{

template<typename ResultType>
class Result
{
public:
    Result(ResultType&& result)
        : m_result(std::move(result)) {}

    Result(const std::exception_ptr& e)
        : m_exception(e) {}

    Result(const std::exception& e)
        : m_exception(std::make_exception_ptr(e)) {}

    ResultType& getOrThrow()
    {
        if(m_exception)
        {
            std::rethrow_exception(m_exception);
        }
        return m_result;
    }

private:
    ResultType m_result;
    std::exception_ptr m_exception;
};


template<>
class Result<void>
{
public:
    Result() {}

    Result(const std::exception_ptr& e)
        : m_exception(e) {}

    Result(const std::exception& e)
        : m_exception(std::make_exception_ptr(e)) {}

    void getOrThrow()
    {
        if(m_exception)
        {
            std::rethrow_exception(m_exception);
        }
    }

private:
    std::exception_ptr m_exception;
};


}