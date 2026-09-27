#pragma once

class Noncopyable
{
public:
    Noncopyable() = default;
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;
};

class Nonmoveable {
public:
    Nonmoveable() = default;
    Nonmoveable(Nonmoveable&& other) = delete;
    Nonmoveable& operator=(Nonmoveable&& other) = delete;
};