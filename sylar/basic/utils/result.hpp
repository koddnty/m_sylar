#pragma once
#include <iostream>
#include <variant>
#include <stdexcept>

namespace m_sylar{




class ResultCreate{

};


template<typename ReturnType, typename ErrorType>
class ResultImpl {
public:
    static ResultImpl<ReturnType, ErrorType> createValue(const ReturnType& value) {
        return ResultImpl<ReturnType, ErrorType>(value, false);
    }
    static ResultImpl<ReturnType, ErrorType> createError(const ErrorType& error) {
        return ResultImpl<ReturnType, ErrorType>(error, true);
    }

public:
    explicit ResultImpl(const ReturnType& value, bool type = false) : m_value_or_error(value) {m_isError = type;}
    explicit ResultImpl(const ErrorType& error, bool type = true) : m_value_or_error(error) { m_isError = type; }

    explicit ResultImpl(ReturnType&& value, bool type = false) : m_value_or_error(std::move(value)) {m_isError = type;}
    explicit ResultImpl(ErrorType&& error, bool type = true) : m_value_or_error(std::move(error)) {m_isError = type; }


    ResultImpl(ResultImpl&& other) noexcept {
        m_isError = other.m_isError;
        m_value_or_error = std::move(other.m_value_or_error);
    }

    ResultImpl& operator=(ResultImpl&& other) noexcept {
        if (this != &other) {
            m_isError = other.m_isError;
            m_value_or_error = std::move(other.m_value_or_error);
        }
        return *this;
    }

    ResultImpl(const ResultImpl& other) = delete; // 禁止拷贝构造
    ResultImpl& operator=(const ResultImpl& other) = delete; // 禁止拷贝赋值


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

    void setValue(const ReturnType& value) {
        m_value_or_error = value;
        m_isError = false;
    }

    void setError(const ErrorType& error) {
        m_value_or_error = error;
        m_isError = true;
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