/*
 * cpu.h -- interface to the simulated CPU
 *
 * This interface is used mainly by the thread library.
 * The only part that is used by application programs is cpu::boot().
 *
 * This class is implemented almost entirely by the infrastructure.  The
 * responsibilities of the thread library are to implement the cpu constructor
 * (and any functions you choose to add) and manage interrupt_vector_table
 * (and any variables you choose to add).
 *
 * You may add new variables and functions to this class.  If you add variables
 * to the cpu class, add them at the *end* of the class definition, and do not
 * exceed the 2 KB total size limit for the class.
 *
 * Do not modify any of the given variable and function declarations.
 */

#pragma once

#if !defined(__cplusplus) || __cplusplus < 202000L
#error Please configure your compiler to use C++20 or higher
#endif

#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 12
#error Please use g++ version 12 or higher
#endif

#if defined(__clang__) && defined(__clang_major__) && __clang_major__ < 16
#error Please use clang++ version 16 or higher
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <source_location>
#include <ucontext.h>

/*
 * Added libraries 
 */
#include <queue>
#include <memory>
#include <vector>

using interrupt_handler_t = void (*)();
using thread_startfunc_t = void (*)(uintptr_t);

// RAII handling of interrupts
class kernel_guard {
public:
    kernel_guard(); 
    ~kernel_guard(); 

    kernel_guard(const kernel_guard&) = delete;
    kernel_guard& operator=(const kernel_guard&) = delete;
    kernel_guard(kernel_guard&&) = delete;
    kernel_guard& operator=(kernel_guard&&) = delete;
}; // kernel_guard

// Similar to kernel guard, but is meant for when running user code so interrupt handling is flipped
class user_guard {
public:
    user_guard(); 
    ~user_guard(); 

    user_guard(const user_guard&) = delete;
    user_guard& operator=(const user_guard&) = delete;
    user_guard(user_guard&&) = delete;
    user_guard& operator=(user_guard&&) = delete;
};

/*
 * Status Enum
 *
 * Determines the status of the TCB
 *
 * Running: Thread is currently running stream of execution
 * (INVARIANT FOR RUNNING): Only one thread can be set to 'Running' per CPU
 *
 * Ready: Thread is currently on the ready queue
 *
 * Blocked: Thread cannot run due to waiting for some condition, lock, or thread to finish
 *
 * Finished: Thread has completed stream of execution
 */
enum class Status : uint8_t {Null = 0, READY, RUNNING, BLOCKED, FINISHED};

/* 
 * Thread Control Block (TCB)
 * 
 * Contains a stack allocated of STACK_SIZE
 * Contains a ucontext_t pointer of the thread's context
 * 
 */
struct TCB {
    TCB(); // TCB constructor

    Status status; // status of the TCB
    uint32_t id; // process id of the TCB
    std::unique_ptr<char[]> stk;
    std::shared_ptr<ucontext_t> uc;
    std::queue<std::shared_ptr<TCB>> join_q; 
}; 

class cpu {
public:
    /*
     * cpu::boot() starts all CPUs in the system.  The number of CPUs is
     * specified by num_cpus.
     * One CPU will call cpu::cpu(func, arg); the other CPUs will call
     * cpu::cpu(nullptr, nullptr).
     *
     * On success, cpu::boot does not return.
     *
     * async, sync, random_seed configure each CPU's generation of timer
     * interrupts (which are only delivered if interrupts are enabled).
     * Timer interrupts in turn cause the current thread to be preempted.
     *
     * The sync and async parameters can generate several patterns of interrupts:
     *
     *     1. async = true: generate interrupts asynchronously every 1 ms.
     *        These are non-deterministic.  
     *
     *     2. sync = true: generate synchronous, pseudo-random interrupts on each
     *        CPU.  You can generate different (but repeatable) interrupt
     *        patterns by changing random_seed.
     *
     * An application will be deterministic if num_cpus=1 && async=false.
     */
    static void boot(unsigned int num_cpus, thread_startfunc_t func, uintptr_t arg,
                     bool async, bool sync, unsigned int random_seed);

    /*
     * interrupt_disable() disables interrupts on the executing CPU.
     *
     * interrupt_enable() and interrupt_enable_suspend() enable interrupts
     * on the executing CPU.
     *
     * interrupt_enable_suspend() atomically enables interrupts and suspends
     * the executing CPU until it receives an interrupt from another CPU.
     * The CPU will ignore timer interrupts while suspended.
     *
     * These functions should only be called by the thread library (not by
     * an application).  They are static member functions because they always
     * operate on the executing CPU.
     *
     * Each CPU starts with interrupts disabled.
     */
    static void interrupt_disable();
    static void interrupt_enable();
    static void interrupt_enable_suspend();

    /*
     * interrupt_send() sends an inter-processor interrupt to the specified CPU.
     * E.g., c_ptr->interrupt_send() sends an IPI to the CPU pointed to by c_ptr.
     */
    void interrupt_send();

    /*
     * The interrupt vector table specifies the functions that will be called
     * for each type of interrupt.  There are two interrupt types: TIMER and
     * IPI.
     */
    static constexpr unsigned int TIMER = 0;
    static constexpr unsigned int IPI = 1;
    interrupt_handler_t interrupt_vector_table[IPI+1];

    static cpu* self();                 // returns pointer to the cpu that
                                        // the calling thread is running on

    /*
     * The infrastructure provides an atomic guard variable, which thread
     * libraries should use to provide mutual exclusion on multiprocessors.
     * The switch invariant for multiprocessors specifies that this guard variable
     * must be true when calling swapcontext.  guard is initialized to false.
     */
    static std::atomic<bool> guard;

    /*
     * Disable the default copy constructor, copy assignment operator,
     * move constructor, and move assignment operator.
     */
    cpu(const cpu&) = delete;
    cpu& operator=(const cpu&) = delete;
    cpu(cpu&&) = delete;
    cpu& operator=(cpu&&) = delete;

    /*
     * The cpu constructor initializes a CPU.  It is provided by the thread
     * library and called by the infrastructure.  After a CPU is initialized, it
     * should run user threads as they become available.  If func is not
     * nullptr, cpu::cpu() also creates a user thread that executes func(arg).
     *
     * On success, cpu::cpu() should not return to the caller.
     */
    cpu(thread_startfunc_t func, uintptr_t arg);

    /************************************************************
    * Add any variables you want here (do not add them above   *
    * interrupt_vector_table).  Do not exceed the 2 KB size    *
    * limit for this class.  Do not add any private variables. *
    *                                                          *
    * Warning: unique_ptr may not pass the is_standard_layout  *
    * assertion on all compilers; use shared_ptr instead.      *
    ************************************************************/
    
    static void guard_acquire(); // acquires cpu guard
    static void guard_release(); // releases gpu guard

    // TODO
    static void ipi_handler(); 

    /*
     * if a thread is available, the cpu will preempt the thread and run the next available thread
     * otherwise, the currently running thread will continue
     */
    static void timer_interrupt_handler();

    /*
     * a cpu is suspended everytime there are no available threads in the ready queue
     *
     * a suspended cpu will set its current thread pointer to null to ensure it does not 
     * have a reference to any thread when suspended
     */
    static void suspend_cpu();

    static void suspend_helper();

    /*
     * for multiprocessors, everytime a thread becomes ready a running cpu will send an IPI 
     * to a sleeping cpu from the 'sleeping_cpu' queue 
     *
     * if there are no cpu's sleeping, the function returns
     */
    static void fetch_cpu();
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
    static void begin_process();

    /*
     * INVARIANT: 
     *              function is called whenever a thread enters a BLOCKED state
     *
     * cpu gets the next available thread from the ready queue 
     * if there are no available threads, then the cpu will suspend
     */
    static void get_next_thread();
    
    /*
     * MODIFIES: thread->status to Status::READY
     * 
     * Pushes a thread onto the ready queue 
     */
    static void push_to_queue(const std::shared_ptr<TCB>& thread);

    /*
     * MODIFIES: cpu::finished_threads by clearing it 
     * 
     * Everytime the thread 'curr' resumes its context after a swapcontext occurs,
     * it will clear the vector of finished threads to ensure memory leakage does not occur
     */
    static void clear_finished_threads(const std::shared_ptr<TCB>& curr);

    /*
     * INVARIANT: 
     *              All threads in finished_threads must have status FINISHED
     */
    inline static std::vector<std::shared_ptr<TCB>> finished_threads;

    /*
     * INVARIANT:
     *              All cpus that are sleeping must have 'curr_thread' set to nullptr
     */
    inline static std::queue<cpu*> sleeping_cpus; 

    /*
     * INVARIANT: 
     *              All threads in ready_threads must have status READY
     */
    inline static std::queue<std::shared_ptr<TCB>> ready_threads; 

    std::shared_ptr<TCB> curr_thread; 
    std::shared_ptr<TCB> suspended_thread; 
    
    static unsigned int num_threads;
    static unsigned int num_cpus; 

    unsigned int cpu_id;

    static bool booted;
    bool suspended = false;
private:    
};

static_assert(sizeof(cpu) <= 2048);
static_assert(std::is_standard_layout<cpu>::value);
static_assert(offsetof(cpu, interrupt_vector_table) == 0);

/*
 * libcpu.o provides a customized version of makecontext, which (1) allows callers
 * to not initialize the context with getcontext, (2) allows callers to pass
 * pointers, and (3) hides the internal members of uc_stack.
 */
void makecontext(ucontext_t *ucp, char* stack, unsigned int stack_size, void (*func)(), int argc, ...);

/*
 * assert_interrupts_disabled() and assert_interrupts_enabled() can be used
 * as error checks inside the thread library.  They will assert (i.e. abort
 * the program and dump core) if the condition they test for is not met.
 */
#define assert_interrupts_disabled()                                             \
                assert_interrupts_private(true, std::source_location::current())
#define assert_interrupts_enabled()                                              \
                assert_interrupts_private(false, std::source_location::current())

/*
 * assert_interrupts_private() is a private function for the interrupt
 * functions.  Your thread library should not call it directly.
 */
void assert_interrupts_private(bool, std::source_location);