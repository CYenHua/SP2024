#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "routine.h"
#include "thread_tool.h"

struct sleepingset sleeping_set;
struct sigaction original_sigalrm,  original_sigtstp;;
// TODO::
// Prints out the signal you received.
// This function should not return. Instead, jumps to the scheduler.
void sighandler(int signum) {
    // Your code here
    if(signum == SIGALRM){
        printf("caught SIGALRM\n");
    }
    if(signum == SIGTSTP){
        printf("caught SIGTSTP\n");
    }
    for(int i = 0; i < THREAD_MAX; i++){
        if(sleeping_set.arr[i] != NULL && sleeping_set.arr[i]->sleeping_time >= 0){
            if(sleeping_set.arr[i]->sleeping_time - time_slice <= 0){
                sleeping_set.arr[i]->sleeping_time = 0;
            }
            else{
                sleeping_set.arr[i]->sleeping_time -= time_slice;
            }
            
        }
        
    }
    longjmp(sched_buf, 1);  
}

void clear_pending_signals() {
    // Temporarily change the signal handler to SIG_IGN (ignore the signal)
    struct sigaction sa_ignore;
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;

    sigaction(SIGALRM, &sa_ignore, &original_sigalrm);
    sigaction(SIGTSTP, &sa_ignore, &original_sigtstp);
    sigaction(SIGALRM, &original_sigalrm, NULL);
    sigaction(SIGTSTP, &original_sigtstp, NULL);
}

void push_to_ready(struct tcb *thread) {
    int tail = (ready_queue.head + ready_queue.size) % THREAD_MAX;
    ready_queue.arr[tail] = thread;
    ready_queue.size++;  
}

void push_to_waiting(struct tcb *thread){
    int tail = (waiting_queue.head + waiting_queue.size) % THREAD_MAX;
    waiting_queue.arr[tail] = thread;
    waiting_queue.size++;  
}

void move_sleeping_to_ready(){
    for(int i = 0; i < THREAD_MAX; i++){
        if(sleeping_set.arr[i] != NULL && sleeping_set.arr[i]->sleeping_time == 0){
            push_to_ready(sleeping_set.arr[i]);
            sleeping_set.arr[i] = NULL;
            sleeping_set.size--;
        }
    }
}

void move_waiting_to_ready() {
    while (waiting_queue.size > 0) {
        struct tcb *thread = waiting_queue.arr[waiting_queue.head];
        if (thread->waiting_for == 2 && rwlock.read_count == 0 && rwlock.write_count == 0) { //write lock
            push_to_ready(thread);
            waiting_queue.head = (waiting_queue.head + 1) % THREAD_MAX;
            waiting_queue.size--;
        }
        else if (thread->waiting_for == 1 && rwlock.write_count == 0) { //read lock
            push_to_ready(thread);
            waiting_queue.head = (waiting_queue.head + 1) % THREAD_MAX;
            waiting_queue.size--;
        }
        else {
            break;
        }
    }
}

void handle_previous_thread(int ret) {
    if (current_thread == idle_thread) {
        return;
    }
    if (ret == 1) { //from the signal handler
        push_to_ready_queue(current_thread);
    } 
    else if (ret == 2) { // from lock
        push_to_waiting_queue(current_thread);
    } 
    else if (ret == 3) { //from sleep
        return;
    } 
    else if (ret == 4) { //from exit
        free(current_thread);
        current_thread = NULL;
        return;
    }
}

void context_switch(){
    if (setjmp(current_thread->env) == 0) {
        longjmp(sched_buf, 1);  
    } 
    else {
        longjmp(current_thread->env, 1); 
    }
}

void print_ready_queue(){
    for(int i = 0; i < ready_queue.size; i++){
        fprintf(stderr, "%d ", ready_queue.arr[ready_queue.head + i]->id);
    }
    fprintf(stderr, "\n");
}



// TODO::
// Perfectly setting up your scheduler.
void scheduler() {
    int ret = setjmp(sched_buf);
    //print_ready_queue();
    alarm(0);
    alarm(time_slice);
    
    if(ret == 0){ //第一次
        thread_create(idle, 0, NULL);
    }

    clear_pending_signals();  //1. Clearing the Pending Signals

   
    move_sleeping_to_ready(); //2. Managing Sleeping Threads
    
    move_waiting_to_ready(); //3. Handling Waiting Threads
    
    handle_previous_thread(ret); //4. Handling Previously Running Threads
    
    //5.Selecting the Next Thread
    if (ready_queue.size > 0) {
        struct tcb *next_thread = ready_queue.arr[ready_queue.head];
        ready_queue.head = (ready_queue.head + 1) % THREAD_MAX;
        ready_queue.size--;
        current_thread = next_thread;
        longjmp(current_thread->env, 1);
    } 
    else if (sleeping_set.size != 0) {
        //fprintf(stderr, "debug145\n");
        current_thread = idle_thread;
        longjmp(current_thread->env, 1);
    } 
    else{
        free(idle_thread);
        idle_thread = NULL;
        return;
    }
    //fprintf(stderr, "debug155\n");
    
    context_switch(); //6.Context Switching
}