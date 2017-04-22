Design Document for Project 1: Threads
======================================

## Group Members

* Ivan Volnov <iv.volnov@yandex.com>

###### Efficient alarm clock

** Data structures and functions **
  * New list data structure `static struct list sleep_list` to be added to _thread.c_
  * New fields to be added to `struct thread`:
    * `struct list_elem slpelem` for `sleep_list` list usage
    * `struct semaphore sleep_sm` semaphore initialised with 0 to wait on during sleeping
    * `int64_t wake_up` number of ticks to pass since OS boot for thread to wake up
  * New function `thread_wake_up` to be added to _thread.c_ to be called in PIT interrupt handler. 

** Algorithms **
When a thread is putten to sleep it:
  * sets its wake up milestone (current tick number + sleep interval)
  * appends itself to the list of sleeping threads
  * blocks itself by downinig(P) its sleep semaphore
When timer chip 8254 interrupt handler is called:
  * for each thread in the system:
      1. checks weather current tick number exceeded the wake up milestone
      2. ups(V) thread's semaphore in case it is ready to wake up
      3. makes current thread to yeild in case any other thread is ready to wake up

** Synchronization **
  * We set only one field in `sruct thread` during a call to `timer_sleep` thus it **may be 
    considered atomic**
  * Each thread uses its own `int64_t wake_up` field thus **no collistions expected**
  * If a thread is preempted before a call to `sema_down` and the sleep interval happens to pass,
    the interrupt handler will up the semaphore thus prevent initial thread from waiting after 
    it'll get the CPU. **No issue here**.
  * As we are planning to use `thread_foreach` function to perform time interval check for each
    thread we **must turn off interrupts** before the call and **turn them on afterwards**.
  * `sleep_list` list is shared among threads and is not thread safe thus we need to 
    **guarantee atomicity during its update**. There is no issue in interrupt handler because it'll
    be run with interrupts off but in `timer_sleep` we need prevent thread preemption during list
    update.

** Rationale **
Alternatives:
  * We can avoid using additional list data structure for sleeping threads but this will lead
    us to iterate over all the threads in the system which can be a sugnificant number.
    It will also require us to run without interrupt for along time thus increasing interrupt
    latency. 
  * We can yeild current thread as soon as we'll find out that a thread needs to ne wakemed.
    The idea is to reduce the time with turned off interrupts(not to check all the threads left).
    In this case if any other threads should've been wakemed too it wiil be handled on the next 
    time tick in the same fashion. In this case we may pospone threads wich may violate
    our sleep interval contract sugnificantly. We also interfier with the work of scheduler.
    We must allow it to follow its policies and optimisations instead.

Currently suggested design basicly creates one global list structure that will store 
pointers only, all the new logic is incapsulated in `thread_wake_up` function that has no 
side effects on the old code and `timer_sleep` busy spinning is substituted with the use of
a standard semaphore. Time and space complexity is linear to the number of sleeping threads.

Functionality can be extended by adding additional logic in `thread_wake_up` function and
additional data to `struct thread`.


###### Priority scheduler

** Data structures and functions **
  * `list_less_func` implementation 'less_priority' and 'less_sem_priority' to be added to 
    _thread.c_ to compare threads and semaphores according to "effective priority"
  * `list_push_back` to be substituted with `list_insert_ordered` in operations on:
    * `struct list ready_list` and `struct list sleep_list` in _thread.c_
  * `list_pop_front` to be substituted with `list_max` in operatons on:
    * `struct list waiters` in `struct semaphore` and `struct condition` from _sync.h_
  * New fields to be added to `struct thread`:
    * `uint64_t eff_priority` to accumulate donations from all waiters in "effective priority"
    * `struct lock *delayer` points to a lock whose holder currently blocks this thread
  * New fields to be added to `struct lock`:
    * `int max_priority` to 
  * `lock_acquire` and `lock_release` to be extended with "priority donation" logic

** Algorithms **
  1. Scheduler:
     When a new thread is added to ready list it is inserted in a sorted fashion according to its
     "effective priority". When "effective priority" changes the thread from ready list must be 
     reinserted. The scheduler logic stays the same.
  2. Synchronization primitives:
     When a condition variable is signaled it ups one of its inner semaphores. The semaphore that
     it pops from the list must be the one that has a waiting thread with the highest priority.
     When a semaphore is uped(V) it unblocks a thread from its waiting threads list by poping 
     the one with max priority. The remaining semaphore functionality is build on top of scheduler
     thus has the same "effective priority" properties as the scheduler. All the other primitives 
     are based on semaphores.
     Nevertheless when a condition variable is signaled it need to up a semaphore from waiting list
     thus it must be the one whose waiter has the highest "effective priority".   
  3. Priority donation:
     Each thread'spriority accumulates donations from all its locks but is constrained from above 
     by the maximum priority.
     Each lock remembers the highest donation from the whole chain fo waiters.
     Each thread remembers the lock on which it is currently blocked.
     When a lock is aquired:
       1. thread's delayer is forgotten
     When lock acquisition is unsuccessfull:
       1. If current thread priority is higher than the holder's:
         * lock's donation is incremented by the difference between the holders priority and
           current thread's priority.
         * holder's priority is incremented by the difference between its priority and current
           thread's priority
       2. A thread remembers this lock as its delayer
       3. If the holder has a delayer lock and its holder has a lower priority:
         * delayer's holder's priority is increased by the difference between it and current 
           thread's priority
     When a lock is released:
       1. its donation is substructed from the holder's priority and 
       2. if a thread becomes not the first one in the ready list then it must yeild. 
     
** Synchronization **
  * Operations on lists of `struct thread` are performed inside functions that disable interrupts:
    * `sema_up`
    * `sema_down`
    * `thread_yeild`
    * `thread_unblock`
    * `thread_exit`
    thus require **no additional synchronization**.
  * In `lock_acquire` when can only execute our new "priority donation" related logic before
    `sema_down` call because the current thread might be bloked in case a semaphore is 0. Thus
    that logic **must run in interrupt free mode**. We can not delegate this job to the semaphore
    itself because it has no notion of a thread owning the lock.
  * When a thread gets CPU it must set its
    
** Rationale **
Alternatives:
  * We can keep waiting and ready lists unsorted and iterate over them each time we need the highest
    priority `list_elem`. In this casse during the "priority donation" we just reset threads' 
    priorities and no overhead on keeping lists sorted. Basicly each pop operation on list is
    caused by the insert on the past so in general the either we sort on insert o search for the
    highest priority element on pop we do it O(n) and equally oftenly. The problem here that search
    on pop is **always** linear while insert into an ordered list might not. The question here is
    how much lesser is the amortized complexity of insert compared to O(n) of pop. The intuition is
    that most of the threads are of equal initial default priority. They form a tail in the end of
    the list wich we ignore during insert iteration(we just insert `list_elem` when it is equal to 
    the adjacent one). If the number of threads with custom priorities is bigger than default than
    this approach is worser. This might happen when users code exploit priorities feature a lot
    with high despertion in priorities or when a lot of nested locks form threads with unique 
    donated priorities. Euristics is that those cases are unlikely. The same assumptions allow us
    to believe that overhead of "priority donation" will be amortised by the reduced cost on the
    common cases.
  
Suggested solution is mostly based on the current infrastructure for synchronisation and thread 
management. It basicly just utilises the properties of sorted list thus memory complexity is not 
increased. Legacy code interfaces those list structures in the same way while new logic adds linear
computational cost overhead. Thread and lock structures are agumented with a new field each(a pointer 
and integer) wich does not radically change the memory footprint of the kernel. Memory management is 
not effected because the solution does not dynamically allocate any memory or in any way interfere 
into the threads' lifecycle in the context of memory deallocation except the case when explained in
synchronization section. 
  

###### Multilevel feedback queue scheduler

** Data structures and functions **
  * `static struct fixed_point_t load_avg` to be added to _thread.c_ for system wide load average
  * New fields to be added to `struct thread`:
    * `uint8_t niceness` to sore thread's niceness value <=20 & >= -20
    * `struct fixed_point_t recent_cpu` to store recent cpu moving average
  * functions to be implemented in _thread.c_:
    * `thread_get_nice`
    * `thread_set_nice`
    * `thread_get_recent_cpu` multiplies by a 100 and rounds to int
    * `thread_get_load_avg` multiplies by a hundred and rounds to int
  * functions to be added to _thread.c_
    * `thread_update_priority` calculates priority for all threads
    * `thread_update_recent_cpu` calculates recent cpu of all threads
    * `thread_update_load_avg` calculates load average of the system(non idle threads)

** Algorithms **
  * When system boot `load_avg` is initialised with 0.
  * When advanced scheduling is chosen "priority donation" must be turned off:
    * `list_less_func` implementation `thread_compare` must use current `priority` instead of 
      "effective priority"
    * `lock_acquire`, `lock_try_acquire` and `lock_release` must not execute "priority donation"
      logic 
    * `thread_get_priority` and `thread_set_priority` must return current `priority` value
  * When PIT interrupt handler is called it:
    * increments recent cpu field by 1 for currently running not idle thread
    * if a number of system ticks is divisible by 4 the for each thread its `priority` is 
      recalculated. Threads must be reinserted into their lists with order. 
    * if a number of system ticks is divisible by `TIMER_FREQ` then:
      1. for each thread its `recent_cpu` is recalculated. Threads must be reinserted with order.
      2. `load_avg` is recalculated
    

** Synchronization **

** Rationale **

######  Design Document Additional Questions
1. answer 1
2.timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|--------------
 0          |      |      |      |      |      |      |
 4          |      |      |      |      |      |      |
 8          |      |      |      |      |      |      |
12          |      |      |      |      |      |      |
16          |      |      |      |      |      |      |
20          |      |      |      |      |      |      |
24          |      |      |      |      |      |      |
28          |      |      |      |      |      |      |
32          |      |      |      |      |      |      |
36          |      |      |      |      |      |      | 
3. answer 3

