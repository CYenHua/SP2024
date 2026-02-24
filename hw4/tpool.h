#pragma once

#include <pthread.h>




typedef int** Matrix;
typedef int* Vector;

typedef struct request{
    Matrix a;
    Matrix b;
    Matrix c;
    Matrix bt;
    int num_works;
    int remaining_works;
    struct request *next;
} request;



typedef struct work {
    Matrix a;
    Matrix b;
    Matrix c;
    int start; 
    int end;   
    request* parent_request;
    struct work *next;
} work;

typedef struct queue {
    void* head;             
    void* tail;            
    int size;               
    pthread_mutex_t mutex;  
    pthread_cond_t cond;    
    int stop;
} queue_t;



typedef struct tpool {
    pthread_t* backend_threads;  
    pthread_t frontend_thread;   
    int num_threads;             
    int active_requests;        
    int matrix_size;             
    queue_t *request_queue;       
    queue_t *work_queue;          
    pthread_mutex_t sync_mutex;  // 用於同步請求完成的鎖
    pthread_cond_t sync_cond;    // 用於同步請求完成的條件變量                  
} tpool;





struct tpool* tpool_init(int num_threads, int n);
void tpool_request(struct tpool*, Matrix a, Matrix b, Matrix c, int num_works);
void tpool_synchronize(struct tpool*);
void tpool_destroy(struct tpool*);
int calculation(int n, Vector, Vector);  // Already implemented
