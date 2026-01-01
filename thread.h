/*
 * thread.h -- interface to the thread library
 *
 * This file should be included by the thread library and by application
 * programs that use the thread library.
 * 
 * You may add new variables and functions to this class.
 *
 * Do not modify any of the given function declarations.
 */

#pragma once

#include <cstdint>

#include "cpu.h"

#if !defined(__cplusplus) || __cplusplus < 202000L
#error Please configure your compiler to use C++20 or higher
#endif

#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 12
#error Please use g++ version 12 or higher
#endif

#if defined(__clang__) && defined(__clang_major__) && __clang_major__ < 16
#error Please use clang++ version 16 or higher
#endif

static constexpr unsigned int STACK_SIZE=262144;// size of each thread's stack in bytes

using thread_startfunc_t = void (*)(uintptr_t);

class thread {
public:
    thread(thread_startfunc_t func, uintptr_t arg); // create a new thread
    ~thread() = default;

    void join();                                // wait for this thread to finish

    static void yield();                        // yield the CPU
    
    /*
    * Disable the copy constructor and copy assignment operator.
    */
    thread(const thread&) = delete;
    thread& operator=(const thread&) = delete;
    
    /*
    * Move constructor and move assignment operator.  Implementing these is
    * optional in Project 2.
    */
    thread(thread&&);
    thread& operator=(thread&&);
private: 
    friend class cpu;


    /* 
     * thread_execution: Function wrapper for the thread.
     * 
     * It runs the user code of the thread and handles the thread after its 
     * stream of execution ends
     * 
     * Calls func(arg) to begin running user code
     *
     * After func(arg) returns, it sets thread_tcb's status to finished
     * After updating the status of the thread, the cpu 
     * looks for the next available thread to run and begins its process
     *
     * otherwise, it will suspend
     * 
     */
    static void thread_execution(thread_startfunc_t func, uintptr_t arg);

    std::weak_ptr<TCB> this_thread; // Store the TCB during thread constructor
};