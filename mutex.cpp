// WORKING code for the mutex class

#include <cassert>
#include <stdexcept>

#include "cpu.h"
#include "mutex.h"

/***************************************************************************************************
 *                                              Mutex                                              *
 ***************************************************************************************************/

mutex::mutex() : thread_holding_lock(-1), free(true)
{} // mutex::mutex()

/*
 * internal lock: helper function to mutex lock
 *
 * interrupts are disabled; can be used by the OS in cv::wait()
 *
 * modifies thread_holding_lock
 */
void mutex::internal_lock() {
    assert_interrupts_disabled();
    assert(cpu::guard == true);

    assert(cpu::self()->curr_thread && "Current thread calling lock is not null");

    if (!free) {
        // Confirm that the current thread has not finished s.o.e
        assert(cpu::self()->curr_thread->status != Status::FINISHED || cpu::self()->curr_thread->status != Status::READY);
        
        // printf("\t\t\t\t(MUTEX LOCK): cpu <%d> thread<%d> did not acquire mutex; pushed to waiting queue\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
        
        cpu::self()->curr_thread->status = Status::BLOCKED;
        assert(cpu::self()->curr_thread->status == Status::BLOCKED);
        waiting_threads.push(cpu::self()->curr_thread);

        cpu::get_next_thread();
    } else {
        thread_holding_lock = static_cast<int>(cpu::self()->curr_thread->id); // review
        free = false;
        // printf("\t\t\t\t(MUTEX LOCK): <cpu %d> Lock was acquired by thread <%d>\n", cpu::self()->cpu_id, thread_holding_lock);
    }
} // mutex::internal_lock();

/*
 * internal unlock: helper function to mutex unlock
 *
 * interrupts are disabled; can be used by the OS in cv::wait()
 *
 * modifies thread_holding_lock
 */
void mutex::internal_unlock() {
    assert_interrupts_disabled();
    assert(cpu::guard == true);

    if (thread_holding_lock != static_cast<int>(cpu::self()->curr_thread->id)) {
        throw std::runtime_error("Unlock called by thread not holding mutex\n");
    }

    assert(cpu::self()->curr_thread && "Current thread calling lock is not null");
    free = true;

    if (!waiting_threads.empty()) {
        auto waiting_thread = waiting_threads.front();
        waiting_threads.pop();

        // printf("\t\t\t\t(MUTEX UNLOCK) <cpu %d thread %d> thread being popped from mutex waiting queue\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
        assert(waiting_thread->status != Status::FINISHED);
        assert(waiting_thread.get() != nullptr && "Waiting thread after unlock is null");
        
        thread_holding_lock = static_cast<int>(waiting_thread->id);
        free = false;
        
        // printf("\t\t\t\t(MUTEX UNLOCK): <cpu %d thread %d> thread<%d> popped from waiting queue and pushed onto ready queue after waiting for a lock\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id, waiting_thread->id);
        cpu::push_to_queue(waiting_thread);
    }
} // mutex::internal_unlock()

void mutex::lock() {
    kernel_guard kg;
    internal_lock();
} // mutex::lock()

void mutex::unlock() {
    kernel_guard kg;
    internal_unlock();
} // mutex::unlock()
