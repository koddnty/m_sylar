#pragma once
#include <memory>
#include <iostream>

namespace m_sylar{

template <typename T, typename X = void, int N = 0>
class Singleton{
public:
    static T* GetInstance () {
        static T c;
        return &c;
    }
private:

};

template <typename T, typename X, int N = 0>
class SingletonPtr{
public:
    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> v(new T);
        return v;
    }
};

}