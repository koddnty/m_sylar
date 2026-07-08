#pragma once
#include <iostream>
#include <variant>
#include <stdexcept>

namespace m_sylar{


template<typename ReturnType, typename ErrorType>
class Result {
public:
    Result(const ReturnType& value) : m_value_or_error(value) {m_isError = false;}
    Result(const ErrorType& error) : m_value_or_error(error) { m_isError = true; }

    Result(ReturnType&& value) : m_value_or_error(std::move(value)) {m_isError = false;}
    Result(ErrorType&& error) : m_value_or_error(std::move(error)) {m_isError = true; }


    Result(Result&& other) noexcept {
        m_isError = other.m_isError;
        m_value_or_error = std::move(other.m_value_or_error);
    }

    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            m_isError = other.m_isError;
            m_value_or_error = std::move(other.m_value_or_error);
        }
        return *this;
    }

    Result(const Result& other) = delete; // 禁止拷贝构造
    Result& operator=(const Result& other) = delete; // 禁止拷贝赋值


    bool isError() const {
        return m_isError;
    }

    ReturnType& getValue(){
        if (m_isError) throw std::runtime_error("No value");
        return std::get<ReturnType>(m_value_or_error);
    }

    const ReturnType& getValue() const {
        if (m_isError) throw std::runtime_error("No value");
        return std::get<ReturnType>(m_value_or_error);
    }

    ErrorType& getError() {
        if (!m_isError) throw std::runtime_error("No error");
        return std::get<ErrorType>(m_value_or_error);;
    }
    const ErrorType& getError() const {
        if (!m_isError) throw std::runtime_error("No error");
        return std::get<ErrorType>(m_value_or_error);;
    }

public:
    explicit operator bool() const {
        return !m_isError;
    }
    

private:
    std::variant<ReturnType, ErrorType> m_value_or_error;
    bool m_isError = false;
};

}