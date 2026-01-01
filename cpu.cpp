// WORKING code for the cpu class

#include <cassert>

#include "cpu.h"
#include "thread.h"

/***************************************************************************************************
 *                                                TCB                                              *
 ***************************************************************************************************/

 // TCB constructor
TCB::TCB() : 
    status(Status::Null),  
    id(cpu::num_threads++), 
    stk(std::make_unique_for_overwrite<char[]>(STACK_SIZE)),
    uc(std::make_shared<ucontext_t>())
{} // TCB()

/***************************************************************************************************
 *                                           Kernel Guard                                          *
 ***************************************************************************************************/

// Disables interrupts
kernel_guard::kernel_guard() {
    cpu::interrupt_disable();
    cpu::guard_acquire();
} // kernel_guard()

// Enables interrupts
kernel_guard::~kernel_guard() {
    cpu::guard_release();
    cpu::interrupt_enable();
} // ~kernel_guard()

/***************************************************************************************************
 *                                            User Guard                                           *
 ***************************************************************************************************/

// Enables interrupts
user_guard::user_guard() {
    cpu::guard_release();
    cpu::interrupt_enable();
} // user_guard()

// Disables interrupts
user_guard::~user_guard() {
    cpu::interrupt_disable();
    cpu::guard_acquire();
} // ~user_guard()

/***************************************************************************************************
 *                                               CPU                                               *
 ***************************************************************************************************/

bool cpu::booted = false;
unsigned int cpu::num_cpus = 0;
unsigned int cpu::num_threads = 0;

void cpu::guard_acquire() {
    assert_interrupts_disabled();
    while (guard.exchange(true)) {}
} // cpu::guard_acquire()

void cpu::guard_release() {
    assert_interrupts_disabled();
    guard.store(false);
} // cpu::guard_release()

/*
 * MODIFIES:
 *              'curr_thread' to next available thread 
 *
 * INVARIANT: 
 *              'curr_thread' must be a nullptr
 *
 * a suspended cpu wakes up and acquires a ready thread
 * if no threads are ready, the cpu suspends again
 *
 */
void cpu::ipi_handler() {
    cpu::interrupt_disable();
    cpu::guard_acquire();
    
    if (!cpu::ready_threads.empty()) {
        auto prev = cpu::self()->curr_thread;
        cpu::self()->curr_thread = cpu::ready_threads.front();
        cpu::ready_threads.pop();

        assert(cpu::self()->curr_thread->status == Status::READY);
        cpu::self()->curr_thread->status = Status::RUNNING;
        swapcontext(prev->uc.get(), cpu::self()->curr_thread->uc.get());
    }
} // cpu::ipi_handler()

/*
 * if a thread is available, the cpu will preempt the thread and run the next available thread
 * otherwise, the currently running thread will continue
 */
void cpu::timer_interrupt_handler() {
    {
        kernel_guard kg;
        if (cpu::self()->curr_thread == cpu::self()->suspended_thread) {
            return;
        }
    }

    // printf("\t\t\t\t(TIMER ISR): cpu<%d> thread<%d> interrupted by timer, calling thread yield\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
    thread::yield();
} // cpu::timer_interrupt_handler()

/*
 * MODIFIES:
 *              cpu::self()->curr_thread
 *
 * a cpu is suspended everytime there are no available threads in the ready queue
 *
 * a suspended cpu will set its current thread pointer to null to ensure it does not 
 * have a reference to any thread when suspended
 */
void cpu::suspend_cpu() {   
    assert_interrupts_disabled(); 
    if (cpu::self()->curr_thread) {
        // printf("\t\t\t\t(SUSPEND): swapping context from current cpu<%d> thread<%d>  to suspend_ctx\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
        auto prev = cpu::self()->curr_thread;
        cpu::self()->curr_thread = cpu::self()->suspended_thread;

        assert(cpu::self()->curr_thread && cpu::self()->curr_thread->uc.get());
        assert(cpu::self()->curr_thread->stk.get());
        assert(prev);
        assert(prev->uc.get());
        swapcontext(prev->uc.get(), cpu::self()->curr_thread->uc.get());
    } else {
        // printf("\t\t\t\tcpu starting without a thread, and no threads are available, suspending\n");
        cpu::self()->curr_thread = cpu::self()->suspended_thread;
        setcontext(cpu::self()->curr_thread->uc.get());
    }
} // cpu::suspend_cpu()

void cpu::suspend_helper() {
    while (true) {
        assert_interrupts_disabled();
        
        sleeping_cpus.push(cpu::self());

        cpu::guard_release();
        cpu::interrupt_enable_suspend();
    }
}


/*
 * for multiprocessors, everytime a thread becomes ready a running cpu will send an IPI 
 * to a sleeping cpu from the 'sleeping_cpu' queue 
 *
 * if there are no cpu's sleeping, the function returns 
 */
void cpu::fetch_cpu() {
    assert_interrupts_disabled();

    if (!cpu::sleeping_cpus.empty()) {
        auto next_cpu = sleeping_cpus.front();
        sleeping_cpus.pop();

        assert(next_cpu != cpu::self());
        assert(next_cpu->curr_thread == next_cpu->suspended_thread);
        assert(next_cpu->curr_thread != cpu::self()->suspended_thread);
    
        // printf("\t\t\t\t(KERNEL): <cpu %d> waking up CPU %d through an interprocessor interrupt\n", cpu::self()->cpu_id, next_cpu->cpu_id);
    
        next_cpu->interrupt_send();
    }
}

/*
 * (MODIFIES) cpu::self()->curr_thread 
 *
 * begin_process is the function called by all cpu's in their cpu constructor
 *
 * a cpu will look to see if there are any ready threads
 *
 * if a thread is available, begin_process() will update curr_thread
 * and load a threads context, beginning that thread's execution
 *
 * otherwise, if no threads are available, the cpu will suspend (related to multiprocessor)
 *
 * NOTE:
 * since we make a call to setcontext, the cpu will never return back to this function
 *
 */
 void cpu::begin_process() {
    if (!cpu::ready_threads.empty()) {

        cpu::self()->curr_thread = cpu::ready_threads.front();
        cpu::ready_threads.pop();
        
        assert(cpu::self()->curr_thread.get());
        cpu::self()->curr_thread->status = Status::RUNNING;
        setcontext(cpu::self()->curr_thread->uc.get());
    } else {
        cpu::suspend_cpu();
    }
 } // cpu::begin_process()

/*
 * INVARIANT: 
 *              function is called whenever a thread enters a BLOCKED state
 *
 * cpu gets the next available thread from the ready queue 
 * if there are no available threads, then the cpu will suspend
 */
void cpu::get_next_thread() {

    assert_interrupts_disabled();

    if (!cpu::ready_threads.empty()) {
        assert(cpu::self()->curr_thread.get());

        auto prev                   = cpu::self()->curr_thread; // current thread running
        cpu::self()->curr_thread    = cpu::ready_threads.front(); // next thread to run 

        assert(cpu::self()->curr_thread.get());
        cpu::ready_threads.pop();
        
        assert(cpu::self()->curr_thread->status == Status::READY);
        assert(prev->status == Status::BLOCKED);
       
        // printf("\t\t\t\t(THREAD YIELD) <cpu %d> swappping context from thread %d to thread %d\n", cpu::self()->cpu_id, prev->id, cpu::self()->curr_thread->id);
       
        cpu::self()->curr_thread->status = Status::RUNNING;
        swapcontext(prev->uc.get(), cpu::self()->curr_thread->uc.get());
        assert_interrupts_disabled();

        cpu::clear_finished_threads(prev);
    } else {
        cpu::suspend_cpu();
    } 
} // cpu::get_next_thread()

/*
 * MODIFIES: thread->status to Status::READY
 * 
 * Pushes a thread onto the ready queue 
 */
void cpu::push_to_queue(const std::shared_ptr<TCB>& thread) {

    assert_interrupts_disabled();

    // printf("\t\t\t\t(KERNEL): cpu<%d> pushing thread<%d> onto the ready queue\n", cpu::self()->cpu_id, thread->id);

    assert(thread.get() && "the thread being pushed to queue was a null pointer\n");
    assert(thread->status != Status::FINISHED && "A finished thread attempted to be enqeued onto the ready ready\n");
    assert(thread->status != Status::READY && "the thread being pushed to the ready queue has been enqueued\n");
    assert(thread->status == Status::RUNNING || thread->status == Status::BLOCKED || thread->status == Status::Null);

    thread->status = Status::READY;
    cpu::ready_threads.push(thread);

    cpu::fetch_cpu();
} // cpu::push_to_queue() 

/*
 * MODIFIES: cpu::finished_threads by clearing it 
 * 
 * Everytime the thread 'curr' resumes its context after a swapcontext occurs,
 * it will clear the vector of finished threads to ensure memory leakage does not occur
 */
void cpu::clear_finished_threads(const std::shared_ptr<TCB>& curr) {
    assert_interrupts_disabled();

    // printf("\t\t\t\t(KERNEL): cpu<%d> thread<%d> clearing all finished threads\n", cpu::self()->cpu_id, cpu::self()->curr_thread->id);
    for (auto finished_thread : cpu::finished_threads) {
        assert(finished_thread->status == Status::FINISHED);
        assert(finished_thread.get() != curr.get());
        finished_thread.reset();
    }
}

/*
 * The cpu constructor initializes a CPU.  It is provided by the thread
 * library and called by the infrastructure.  After a CPU is initialized, it
 * should run user threads as they become available.  If func is not
 * nullptr, cpu::cpu() also creates a user thread that executes func(arg).
 *
 * On success, cpu::cpu() should not return to the caller.
 */
cpu::cpu(thread_startfunc_t func, uintptr_t arg) {
    assert_interrupts_disabled();
    cpu::guard_acquire();

    booted = true;
    
    cpu_id = num_cpus++;
    // printf("\t\t\t\t(KERNEL): cpu<%d> created in cpu::cpu\n", cpu_id);
    
    interrupt_vector_table[TIMER]   = cpu::timer_interrupt_handler;
    interrupt_vector_table[IPI]     = cpu::ipi_handler;

    suspended_thread = std::make_shared<TCB>();
    makecontext(suspended_thread->uc.get(),
                suspended_thread->stk.get(),
                STACK_SIZE,
                reinterpret_cast<void(*)()>(cpu::suspend_helper),
                0);

                // cpu creates the first thread
    if (func != nullptr) {
        auto first_thread = std::make_shared<TCB>();

        makecontext(first_thread->uc.get(), 
                    first_thread->stk.get(), 
                    STACK_SIZE, 
                    reinterpret_cast<void(*)()>(thread::thread_execution),
                    2, func, arg);

        // Check state of tcb
        assert(first_thread.get() != nullptr);

        cpu::push_to_queue(first_thread);
    }

    /// IMPORTANT: 
    // Last implementation we took the naive approach of creating a cpu as its own "TCB"
    // and I think that caused alot of problematic issues during the multiprocessor implementation
    // Now the approach that we will take is that the cpu will act like the parent of each thread 
    // that is available, and supervise it through its process as it runs its stream of execution
    // It will be more like a network of threads under a cpu instead of one big scheduler block 
    // that we tried before 
    // Hopefully this will handle the issues we saw during the multiprocessor implementation 
    cpu::begin_process();

} // cpu()
