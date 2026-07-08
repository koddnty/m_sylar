#pragma once
#include <memory>
#include <iostream>

namespace m_sylar{

template <typename T, typename X = void, int N = 0>
class Singleton{
public:
    static T* GetInstance () {
        static T c;
        c.init();
        return &c;
    }
};

template <typename T, typename X, int N = 0>
class SingletonPtr{
public:
    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> v(new T);
        v->init();
        return v;
    }
};

}