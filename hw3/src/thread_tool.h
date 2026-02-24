#ifndef THREAD_TOOL_H
#define THREAD_TOOL_H

#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>

// The maximum number of threads.
#define THREAD_MAX 100


void sighandler(int signum);
void scheduler();

// The thread control block structure.
struct tcb {
    int id;
    int *args;
    // Reveals what resource the thread is waiting for. The values are:
    //  - 0: no resource.
    //  - 1: read lock.
    //  - 2: write lock.
    int waiting_for; //e.g. read or write lock
    int sleeping_time;
    jmp_buf env;  // Where the scheduler should jump to.
    int n, i, f_cur, f_prev; // TODO: Add some variables you wish to keep between switches.
    int d_p, d_s, q_p, q_s, p_p, p_s;
};



// The only one thread in the RUNNING state.
extern struct tcb *current_thread;
extern struct tcb *idle_thread;

struct tcb_queue {
    struct tcb *arr[THREAD_MAX];  // The circular array.
    int head;                     // The index of the head of the queue
    int size;
};
struct sleepingset {
     struct tcb *arr[THREAD_MAX];
     int size;
};

extern struct tcb_queue ready_queue, waiting_queue;
extern struct sleepingset sleeping_set;


// The rwlock structure.
//
// When a thread acquires a type of lock, it should increment the corresponding count.
struct rwlock {
    int read_count;
    int write_count;
};

extern struct rwlock rwlock;

// The remaining spots in classes.
extern int q_p, q_s;

// The maximum running time for each thread.
extern int time_slice;

// The long jump buffer for the scheduler.
extern jmp_buf sched_buf;

// TODO::
// You should setup your own sleeping set as well as finish the marcos below

#define push_to_ready_queue(thread)  \
    ({                            \
        ready_queue.arr[(ready_queue.head + ready_queue.size) % THREAD_MAX] = thread;  \
        ready_queue.size++;         \
    })

#define push_to_waiting_queue(thread)  \
    ({                            \
        waiting_queue.arr[(waiting_queue.head + waiting_queue.size) % THREAD_MAX] = thread;  \
        waiting_queue.size++;         \
    })



#define move_threads() \
    ({                          \
        while (waiting_queue.size > 0) {\
            struct tcb *thread = waiting_queue.arr[waiting_queue.head];\
            push_to_ready_queue(thread);\
            waiting_queue.head = (waiting_queue.head + 1) % THREAD_MAX;\
            waiting_queue.size--;  \
        }\
    })


#define thread_create(func, t_id, t_args)   \
    ({                                   \
        func(t_id, t_args);\
    })




#define thread_setup(t_id, t_args)                                      \
    ({                                                              \
        printf("thread %d: set up routine %s\n", t_id, __func__);    \
        struct tcb* new_thread = malloc(sizeof(struct tcb));\
        new_thread->id = t_id;\
        new_thread->args = t_args; \
        if(setjmp(new_thread->env) == 0){ \
            if (t_id == 0) {                                                \
                idle_thread = new_thread;                              \
            }                                      \
            else {                                                      \
                push_to_ready_queue(new_thread);                       \
            } \
            return;                                           \
        }     \
    })


#define thread_yield()                       \
    ({                                     \
        setjmp(current_thread->env); \
        sigset_t set;                    \
        sigemptyset(&set);               \
        sigaddset(&set, SIGTSTP);        \
        sigprocmask(SIG_UNBLOCK, &set, NULL); \
        sigprocmask(SIG_BLOCK, &set, NULL); \
        /* Unblock and block SIGALRM */   \
        sigemptyset(&set);               \
        sigaddset(&set, SIGALRM);         \
        sigprocmask(SIG_UNBLOCK, &set, NULL); \
        sigprocmask(SIG_BLOCK, &set, NULL); \
    })


#define read_lock()                                      \
    ({              \
        while(1){  \
            if(rwlock.write_count > 0) {                  \
            /* If write lock is held, wait in waiting queue */ \
                current_thread->waiting_for = 1;              \
                if (setjmp(current_thread->env) == 0) {       \
                    longjmp(sched_buf, 2); \
                }\
            }   \
            else{ \
                rwlock.read_count++;                             \
                break;  \
            }                                              \
        }                                   \
    })


#define write_lock()                                     \
    ({                                               \
        while(1){ \
            if(rwlock.read_count > 0 || rwlock.write_count > 0){ \
                current_thread->waiting_for = 2;              \
                if (setjmp(current_thread->env) == 0) {       \
                    longjmp(sched_buf, 2); \
                } \
            }                                                 \
            else{ \
                 rwlock.write_count = 1;                          \
                 break; \
            } \
        }  \
    })


#define read_unlock()                                   \
    ({                                              \
        rwlock.read_count--;                            \
    })


#define write_unlock()                                  \
    ({                                              \
        rwlock.write_count--;                          \
    })


#define thread_sleep(sec)                                          \
    ({                                                         \
        current_thread->sleeping_time = sec;                        \
        sleeping_set.arr[current_thread->id] = current_thread;\
        sleeping_set.size++;\
        if(setjmp(current_thread->env)== 0){    \
            longjmp(sched_buf, 3);\
        }       \
    })


#define thread_awake(t_id)                                      \
    ({                                                    \
        if(sleeping_set.arr[t_id] != NULL){  \
            sleeping_set.arr[t_id]->sleeping_time = 0;  \
            push_to_ready_queue(sleeping_set.arr[t_id]);\
            sleeping_set.arr[t_id] = NULL; \
            sleeping_set.size--;\
        }                                                  \
    })


#define thread_exit()                                       \
    ({                                                     \
        printf("thread %d: exit\n", current_thread->id);   \
        longjmp(sched_buf, 4);                            \
    })


#endif  // THREAD_TOOL_H