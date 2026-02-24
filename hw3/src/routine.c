#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "thread_tool.h"

void idle(int id, int *args) {
    // TODO:: IDLE ^-^
    thread_setup(id, args);
    printf("thread %d: idle\n", current_thread->id);
    sleep(1);
    thread_yield();
}

void fibonacci(int id, int *args) {
    
    thread_setup(id, args);
    
    current_thread->n = current_thread->args[0];
    for (current_thread->i = 1;; current_thread->i++) {
        if (current_thread->i <= 2) {
            current_thread->f_cur = 1;
            current_thread->f_prev = 1;
        } else {
            int f_next = current_thread->f_cur + current_thread->f_prev;
            current_thread->f_prev = current_thread->f_cur;
            current_thread->f_cur = f_next;
        }
        printf("thread %d: F_%d = %d\n", current_thread->id, current_thread->i,current_thread->f_cur);

        sleep(1);

        if (current_thread->i == current_thread->n) {
            thread_exit();
        } else {
            thread_yield();
        }
    }
}

void pm(int id, int *args) {
    // TODO:: pm ^^--^^
    thread_setup(id, args);
    current_thread->n = current_thread->args[0];
    for (current_thread->i = 1;; current_thread->i++) {
        if (current_thread->i  == 1) {
            current_thread->f_cur = 1;
            current_thread->f_prev = 1;
        } 
        else {
            if(current_thread->i % 2 == 0){
                current_thread->f_cur = (-1) * current_thread->i + current_thread->f_prev;
            }
            else{
                current_thread->f_cur = current_thread->i + current_thread->f_prev;
            }
            current_thread->f_prev = current_thread->f_cur;
        }
        printf("thread %d: pm(%d) = %d\n", current_thread->id, current_thread->i, current_thread->f_cur);

        sleep(1);

        if (current_thread->i == current_thread->n) {
            thread_exit();
        } else {
            thread_yield();
        }
    }

}

void enroll(int id, int *args) {
    // TODO:: enroll !! -^-
    thread_setup(id, args);
    current_thread->sleeping_time = current_thread->args[2];
    current_thread->d_p = current_thread->args[0], current_thread->d_s = current_thread->args[1];
    //Step 1
    printf("thread %d: sleep %d\n", current_thread->id, current_thread->sleeping_time);
    thread_sleep(current_thread->sleeping_time);

    //Step 2
    thread_awake(current_thread->args[3]);
    read_lock(); // Acquire the read lock?
    //int current_q_p = q_p, current_q_s = q_s;
    current_thread->q_p = q_p;
    current_thread->q_s = q_s;
    printf("thread %d: acquire read lock\n", current_thread->id);
    sleep(1);
    thread_yield();

    //Step 3
    read_unlock();
    current_thread->p_p = current_thread->d_p * current_thread->q_p;
    current_thread->p_s = current_thread->d_s * current_thread->q_s;
    printf("thread %d: release read lock, p_p = %d, p_s = %d\n", current_thread->id, current_thread->p_p, current_thread->p_s);
    sleep(1);
    thread_yield();

    //Step 4
    write_lock();
    if(current_thread->p_p == current_thread->p_s){
        if(current_thread->d_p > current_thread->d_s){
            printf("thread %d: acquire write lock, enroll in pj_class\n", current_thread->id);
            q_p--;
        }
        else{
            printf("thread %d: acquire write lock, enroll in sw_class\n", current_thread->id);
            q_s--;
        }
    }
    else if(current_thread->p_p > current_thread->p_s){
        if(q_p > 0){
            printf("thread %d: acquire write lock, enroll in pj_class\n", current_thread->id);
            q_p--;
        }
        else{
            printf("thread %d: acquire write lock, enroll in sw_class\n", current_thread->id);
            q_s--;
        }
    }
    else{
        if(q_s > 0){
            printf("thread %d: acquire write lock, enroll in sw_class\n", current_thread->id);
            q_s--;
        }
        else{
            printf("thread %d: acquire write lock, enroll in pj_class\n", current_thread->id);
            q_p--;
        }
    }
    sleep(1);
    thread_yield();

    //Step 5
    write_unlock();
    printf("thread %d: release write lock\n", current_thread->id);
    sleep(1);
    thread_exit();


}

