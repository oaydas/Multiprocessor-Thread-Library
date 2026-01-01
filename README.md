# Multiprocessor Thread Library

A preemptive, multiprocessor user-level thread library implemented in C++, supporting kernel-style scheduling, inter-processor interrupts (IPIs), and synchronization primitives including threads, mutexes, and condition variables.

This project implements a complete threading runtime for a simulated multi-CPU environment, emphasizing correctness under concurrency, strict scheduling semantics, and safe coordination across multiple processors.

---

## Overview

The library provides a full cooperative + preemptive threading system that runs across multiple simulated CPUs. It includes:

- A CPU runtime that schedules user threads on multiple processors
- A thread abstraction with lifecycle management and joining
- A mutex implementation for mutual exclusion
- A condition variable implementation for blocking and signaling
- Preemptive scheduling via timer interrupts
- Cross-CPU wakeups via inter-processor interrupts (IPIs)

Threads execute as user-level contexts (ucontext) and are multiplexed onto CPUs by the library scheduler.

---

## Major Components

### CPU Runtime (`cpu`)
Responsible for bootstrapping CPUs, handling interrupts, suspending when idle, and dispatching runnable threads.

Key responsibilities:
- Initialize interrupt vector table (timer + IPI handlers)
- Maintain a global ready queue
- Suspend CPUs when no work exists
- Wake sleeping CPUs when new work arrives
- Context switch between threads using `swapcontext`/`setcontext`

### Thread Control Block (TCB)
Each thread is represented by a TCB containing:
- Execution status (READY, RUNNING, BLOCKED, FINISHED)
- Thread id
- Stack allocation
- ucontext state for switching and resuming execution

TCBs are managed via smart pointers to avoid leaks and ensure safe reclamation.

### Thread API (`thread`)
Provides:
- Thread creation and execution wrapper
- Yielding (explicit and preemptive via timer interrupts)
- Blocking/unblocking integration with synchronization primitives
- Joining and lifecycle cleanup

### Synchronization (`mutex`, `cv`)
Implements classical synchronization semantics:
- Mutex provides mutual exclusion and ownership safety
- Condition variable supports waiting, signaling, and broadcast
- Waiting threads block and are later moved to the ready queue in FIFO order

Blocking and waking integrate directly with the scheduler and ready queue.

---

## Scheduling Model

### Ready Queue
Runnable threads are placed into a shared ready queue. When a CPU needs work, it pops the next thread and runs it.

The scheduler ensures:
- Threads transition through READY/RUNNING/BLOCKED/FINISHED states correctly
- Threads only enter the ready queue when valid to run
- Threads are never enqueued twice
- Finished threads are not rescheduled

### Preemption via Timer Interrupts
A timer interrupt triggers preemption by forcing the currently running thread to yield. This ensures fairness and prevents CPU monopolization.

- Timer interrupt handler disables interrupts, enters the kernel critical section, and invokes yield logic
- If no other threads are ready, the current thread continues

### Multiprocessor Support and IPIs
When a thread becomes ready, the runtime may wake a sleeping CPU using an inter-processor interrupt (IPI):

- Sleeping CPUs are tracked in a queue
- When work becomes available, an active CPU sends an IPI to a sleeping CPU
- The IPI handler resumes the CPU and allows it to pull from the ready queue

This allows the system to scale work across CPUs without busy-waiting.

---

## Idle CPU Suspension

When no runnable threads exist, CPUs enter a suspended state:

- CPU switches to a special "suspended thread" context
- The suspended loop releases the global guard and calls `interrupt_enable_suspend()`
- CPU remains asleep until woken by an IPI

This avoids spinning and matches realistic OS behavior when idle.

---

## Concurrency and Safety

### Kernel-Style Global Guard
To ensure correctness across CPUs, a global atomic guard enforces mutual exclusion for scheduler-critical operations:

- `kernel_guard` acquires the guard and disables interrupts
- `user_guard` releases the guard and enables interrupts when returning to user code

This protects shared structures such as:
- Ready queue
- Sleeping CPU queue
- Thread lifecycle cleanup structures

### Interrupt Discipline
All scheduling and shared-structure manipulation occurs with interrupts disabled and the guard held. This prevents races between:
- Timer interrupts
- IPI interrupts
- Concurrent CPU scheduling operations

---

## Resource Management

- Thread stacks are owned and freed automatically via smart pointers
- Finished threads are reclaimed safely after context switches (deferred cleanup)
- No dangling ucontext references
- No rescheduling of finished threads

---

## Technologies Used

- C++ (modern memory management and RAII)
- ucontext-based context switching (`makecontext`, `swapcontext`, `setcontext`)
- Atomic synchronization for the multiprocessor guard
- Interrupt vector table integration (timer + IPI)
- Classic OS scheduling and synchronization design patterns

---

## Summary

This project implements a full multiprocessor thread runtime comparable to a simplified OS kernel scheduler:

- Preemptive scheduling using timer interrupts
- Multiprocessor coordination using IPIs
- Correct blocking/wakeup semantics through mutexes and condition variables
- Safe, disciplined synchronization via RAII guards and atomic locking
- Efficient CPU suspension when idle

It demonstrates deep systems understanding across scheduling, concurrency control, context switching, and synchronization in a realistic multi-CPU setting.
