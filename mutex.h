/*
 * mutex.h -- interface to the mutex class
 *
 * You may add new variables and functions to this class.
 *
 * Do not modify any of the given function declarations.
 */

#pragma once

#include <memory>
#include <queue>

#include "cpu.h"

class mutex {
public:
    mutex();
    ~mutex() = default;

    void lock();
    void unlock();

    /*
     * Disable the copy constructor and copy assignment operator.
     */
    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;

    /*
     * Move constructor and move assignment operator.  Implementing these is
     * optional in Project 2.
     */
    mutex(mutex&&);
    mutex& operator=(mutex&&);

private: 
    friend class cv;

    void internal_lock();
    void internal_unlock();

    // Queue of waiting threads, pointer to thread holding lock and the mutexes status
    std::queue<std::shared_ptr<TCB>> waiting_threads;

    int thread_holding_lock; // thread ID thats holding lock
    bool free;
};