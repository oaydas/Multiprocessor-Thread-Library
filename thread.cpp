// WORKING code for the thread class

#include <cassert>

#include "cpu.h"
#include "thread.h"

/***************************************************************************************************
 *                                              Thread                                             *
 ***************************************************************************************************/
 
 /*
  * Thread constructor
  */
 thread::thread(thread_startfunc_t func, uintptr_t arg) {
    kernel_guard kg;

    assert_interrupts_disabled();
    // printf("\t\t\t\t(THREAD) thread constructor called by cpu<%d> thread<%d>\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
     
    assert(func != nullptr); // fails if a null pointer is passed into 'func'
    assert(cpu::self()->booted);
     
    auto tcb = std::make_shared<TCB>(); // allocate tcb on heap
 
    makecontext(tcb->uc.get(), 
                tcb->stk.get(), 
                STACK_SIZE, 
                reinterpret_cast<void(*)()>(thread::thread_execution), 
                2, 
                func, arg);
 
    assert(tcb.get() != nullptr);

    this_thread = tcb;
 
    // printf("\t\t\t\t(THREAD) thread<%d> created by cpu<%d> thread<%d> and pushed onto ready queue\n", tcb->id, cpu::self()->cpu_id, cpu::self()->curr_thread->id);
    cpu::push_to_queue(tcb);
 } // thread::thread()


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
void thread::thread_execution(thread_startfunc_t func, uintptr_t arg) {
    assert_interrupts_disabled();
    assert(cpu::guard == true);

    // printf("\t\t\t\t(THREAD EXEC): cpu<%d> starting thread<%d> user code\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
    {
        user_guard ug;
        func(arg);
    }    

    assert_interrupts_disabled();
    assert(cpu::guard == true);
    // printf("\t\t\t\t(THREAD EXEC): cpu<%d>, thread <%d> finished stream of execution\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
    

    // Move all threads that were joined back to ready queue to resume execution 
    while (!cpu::self()->curr_thread->join_q.empty()) {
        auto thread = cpu::self()->curr_thread->join_q.front();
        cpu::self()->curr_thread->join_q.pop();
        cpu::push_to_queue(thread);
    }

    // // CPU will now pick up the next available thread immediately instead of returning 
    // // to the scheduler. If no threads are available in the queue then the CPU will suspend

    cpu::self()->curr_thread->status = Status::FINISHED; 
    cpu::finished_threads.push_back(cpu::self()->curr_thread);

    if (!cpu::ready_threads.empty()) {
        auto finished_t             = cpu::self()->curr_thread; // will go out of scope
        cpu::self()->curr_thread    = cpu::ready_threads.front(); // next thread to run 
        cpu::ready_threads.pop();

        // printf("\t\t\t\t(THREAD EXEC): cpu<%d> setting thread<%d> context after becoming current thread pointer\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
        
        cpu::self()->curr_thread->status = Status::RUNNING;
        setcontext(cpu::self()->curr_thread->uc.get());
    } else {
        cpu::suspend_cpu();
    }    
} // thread::thread_execution()    

void thread::yield() {
    kernel_guard kg;

    assert_interrupts_disabled();
    assert(cpu::guard == true);

    // printf("\t\t\t\t(THREAD YIELD) <cpu %d thread %d> thread yielding\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
    
    assert(cpu::self()->booted);
    assert(cpu::self()->curr_thread.get() && "Current tcb is null");

    if (!cpu::ready_threads.empty()) {
        // // printf("\t\t\t\t(THREAD) <cpu %d thread %d> yield performing inplace swap before running\n", cpu::self()->cpu_tcb->id, cpu::self()->curr_tcb->id);
        auto prev                   = cpu::self()->curr_thread; // current thread running
        cpu::self()->curr_thread    = cpu::ready_threads.front(); // next thread to run 
        cpu::ready_threads.pop();

        cpu::push_to_queue(prev);

        // printf("\t\t\t\t(THREAD YIELD) <cpu %d> swappping context from thread %d to thread %d\n", cpu::self()->cpu_id, prev->id, cpu::self()->curr_thread->id);
        
        cpu::self()->curr_thread->status = Status::RUNNING;
        swapcontext(prev->uc.get(), cpu::self()->curr_thread->uc.get());

        // Whenever the yielded thread resumes its context it will clear any finished threads 
        cpu::clear_finished_threads(prev);
    } 
    // printf("\t\t\t\t(THREAD YIELD) <cpu %d thread %d> returning from yield\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
}   // thread::yield();

void thread::join() {
    kernel_guard kg;
    assert_interrupts_disabled();
    assert(cpu::guard == true);

    if(auto temp_this_thread = this_thread.lock()){
        // printf("\t\t\t\t(THREAD JOIN): cpu<%d> thread<%d> join called by thread<%d>\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id, 0);
    // The thread that called join will push current tcb to the join queue and block it
        if (temp_this_thread->status != Status::FINISHED) {
            cpu::self()->curr_thread->status = Status::BLOCKED;
            temp_this_thread->join_q.push(cpu::self()->curr_thread);

          cpu::get_next_thread();
        } 
    }
} // thread::join()