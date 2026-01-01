// WORKING code for the cv class

#include "cv.h"
#include <cassert>
#include "cpu.h"


void cv::wait(mutex& mtx) {
    kernel_guard kg;

    assert_interrupts_disabled();
    assert(cpu::guard == true);

    // printf("\t\t\t\t(CV WAIT): cpu<%d> thread<%d> beginning wait\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
    // // first 3 steps are atomic
    if (mtx.thread_holding_lock == static_cast<int>(cpu::self()->curr_thread->id)) {
        // step 1: release the lock
        mtx.internal_unlock();
        
        // step 2: thread moved to waiting queue
        cpu::self()->curr_thread->status = Status::BLOCKED;
        waiting_threads.push(cpu::self()->curr_thread);
    
        // step 3: go to sleep (AKA get the next thread)
        cpu::get_next_thread();

        // step 4: lock the mutex
        mtx.internal_lock();
    } else {
        throw std::runtime_error("cv::wait() called by thread that did not own mutex");
    }
} // cv::wait()

void cv::signal() {
    kernel_guard kg;
    
    assert_interrupts_disabled();
    assert(cpu::guard == true);
    // printf("\t\t\t\t(CV SIGNAL): cpu<%d> thread<%d> signaling a sleeping thread\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
    if (!waiting_threads.empty()) {
        auto next_thread = waiting_threads.front();
        waiting_threads.pop();

        cpu::push_to_queue(next_thread);
    }
} // cv::signal()

void cv::broadcast() {
    kernel_guard kg;

    assert_interrupts_disabled();
    assert(cpu::guard == true);
    // printf("\t\t\t\t(CV SIGNAL): cpu<%d> thread<%d> broadcasting all sleeping threads\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
    while (!waiting_threads.empty()) {
        auto next_thread = waiting_threads.front();
        waiting_threads.pop();
        cpu::push_to_queue(next_thread);
    }
} // cv::broadcast()