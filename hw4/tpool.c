#include "tpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>


void queue_init(queue_t* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->stop = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void queue_destroy(queue_t* queue) {
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
}

/*void enqueue(queue_t* queue, void* element, int type_size) {
    pthread_mutex_lock(&queue->mutex);
    void* new_element = malloc(type_size);
    memcpy(new_element, element, type_size);
    ((void**)new_element)[type_size / sizeof(void*) - 1] = NULL; // 設置 next 為 NULL

    if (queue->tail == NULL) {
        queue->head = new_element;
        queue->tail = new_element;
    } 
    else {
        ((void**)queue->tail)[type_size / sizeof(void*) - 1] = new_element;
        queue->tail = new_element;
    }
    queue->size++;
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

void* dequeue(queue_t* queue) {
    pthread_mutex_lock(&queue->mutex);

    while (queue->size == 0) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }

    void* element = queue->head;
    queue->head = ((void**)queue->head)[0];

    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->size--;

    pthread_mutex_unlock(&queue->mutex);
    return element;
}*/

struct work *create_work(Matrix a, Matrix b, Matrix c, int start, int end) {
    struct work *new_work = malloc(sizeof(struct work));
    new_work->a = a;       
    new_work->b = b;      
    new_work->c = c;      
    new_work->start = start; 
    new_work->end = end;    
    new_work->next = NULL; 
    return new_work;
}


void enqueue_work(queue_t *queue, struct work *work) {
    pthread_mutex_lock(&queue->mutex);
    work->next = NULL;
    if (queue->tail != NULL) {
        ((struct work*)queue->tail)->next = work;
    } 
    else {
        queue->head = work;
    }
    queue->tail = work;
    queue->size++;
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);

}

struct work *dequeue_work(queue_t *queue) {
    //fprintf(stderr, "debug89\n");
    pthread_mutex_lock(&queue->mutex);
    //fprintf(stderr, "debug91\n");
    while (queue->size == 0 && queue->stop == 0) { 
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    //fprintf(stderr, "debug95\n");

    if (queue->head == NULL) { 
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    struct work* work = (struct work*)queue->head;
    queue->head = work->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->size--;
    pthread_mutex_unlock(&queue->mutex);
    //fprintf(stderr, "debug106\n");
    return work;
}

void enqueue_request(queue_t *queue, request *req) {
    pthread_mutex_lock(&queue->mutex);
    req->next = NULL; 
    if(queue->tail != NULL){
        ((struct request*)queue->tail)->next = req;
    } 
    else{
        queue->head = req;
    }
    queue->tail = req; 
    queue->size++;    
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

request *dequeue_request(queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->size == 0 && queue->stop == 0) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }

    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    request *req = (request *)queue->head;
    queue->head = req->next;      

    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->size--; 
    pthread_mutex_unlock(&queue->mutex);
    return req; 
}



void* backend_thread_func(void* arg) {
    tpool* pool = (tpool*)arg;

    while (1) {
        pthread_mutex_lock(&pool->work_queue->mutex);
        if (pool->work_queue->stop == 1 && pool->work_queue->size == 0) {
            pthread_mutex_unlock(&pool->work_queue->mutex);
            break;
        }
        pthread_mutex_unlock(&pool->work_queue->mutex);
        //fprintf(stderr, "debug159\n");

        work* current_work = dequeue_work(pool->work_queue);

        if (!current_work) {
            continue; // 繼續下一次迴圈
        }
        //if (!current_work) continue;
        for (int idx = current_work->start; idx <= current_work->end; idx++) {
            /*if (current_work == NULL || current_work->a == NULL || current_work->b == NULL || current_work->c == NULL) {
                fprintf(stderr, "Invalid work: current_work=%p, a=%p, b=%p, c=%p\n", (void*)current_work, (void*)current_work->a, (void*)current_work->b, (void*)current_work->c);
                continue; // 跳過這個工作
            }*/
            

            int row = idx / pool->matrix_size;
            int col = idx % pool->matrix_size;
            /*if (row >= n || col >= n) {
                fprintf(stderr, "Invalid matrix access: row=%d, col=%d, n=%d\n", row, col, n);
                continue; // 防止越界
            }*/

            current_work->c[row][col] = calculation(pool->matrix_size, current_work->a[row], current_work->b[col]);
        }

        pthread_mutex_lock(&pool->sync_mutex);
        current_work->parent_request->remaining_works--;
        if (current_work->parent_request->remaining_works == 0) {
            pool->active_requests--;
            free(current_work->parent_request);
            if(pool->active_requests == 0){
                pthread_cond_broadcast(&pool->sync_cond); 
            }
        }
        pthread_mutex_unlock(&pool->sync_mutex);

        free(current_work);
    }
    return NULL;
}

void* frontend_thread_func(void* arg) {
    tpool* pool = (tpool*)arg;

    while (1) { 
        pthread_mutex_lock(&pool->request_queue->mutex);
        if (pool->request_queue->stop == 1 && pool->request_queue->size == 0) {
            pthread_mutex_unlock(&pool->request_queue->mutex);
            break;
        }
        pthread_mutex_unlock(&pool->request_queue->mutex);

        request* current_request = dequeue_request(pool->request_queue);
        if (!current_request) continue;

        int n = pool->matrix_size;
        current_request->remaining_works = current_request->num_works;

        // 轉置 b 矩陣
        current_request->bt = (Matrix)malloc(n * sizeof(int*));
        for (int i = 0; i < n; i++) {
            current_request->bt[i] = (int*)malloc(n * sizeof(int));
        }
        for(int i = 0; i < n; i++){
            for (int j = 0; j < n; j++) {
                current_request->bt[i][j] = current_request->b[j][i];
            }
        }

        // 分割工作
        //divide_work(current_request->a, b_transposed, current_request->c, n, current_request->num_works, pool->work_queue);
        int total_entries = pool->matrix_size * pool->matrix_size;
        int base_size = total_entries / current_request->num_works;
        int remaining_works = total_entries % current_request->num_works;

        int start = 0;
        for (int i = 0; i < current_request->num_works; i++) {
            // 計算每個工作的大小
            int work_size = base_size + (i < remaining_works ? 1 : 0);  // 平分餘數
            int end = start + work_size - 1;  // 計算結束索引

            // 確保範圍不超出總工作量
            if (start >= total_entries) break;
            if (end >= total_entries) end = total_entries - 1;

            // 建立新的工作
            struct work *new_work = create_work(current_request->a, current_request->bt, current_request->c, start, end);
            new_work->parent_request = current_request;

            // 加入工作隊列
            enqueue_work(pool->work_queue, new_work);

            // 更新 start
            start = end + 1;
        }

        //free(current_request);
    }
    return NULL;
}

tpool* tpool_init(int num_threads, int n) {
    tpool* pool = (tpool*)malloc(sizeof(tpool));
    pool->num_threads = num_threads;
    pool->active_requests = 0;
    pool->matrix_size = n;
    
    pool->request_queue = (queue_t*)malloc(sizeof(queue_t));  
    pool->work_queue = (queue_t*)malloc(sizeof(queue_t));  

    queue_init(pool->request_queue);
    queue_init(pool->work_queue);

    pthread_mutex_init(&pool->sync_mutex, NULL);
    pthread_cond_init(&pool->sync_cond, NULL);

    pool->backend_threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->backend_threads[i], NULL, backend_thread_func, (void*)pool);
    }

    pthread_create(&pool->frontend_thread, NULL, frontend_thread_func, (void*)pool);
    return pool;
}

void tpool_request(struct tpool *pool, Matrix a, Matrix b, Matrix c, int num_works) {
    struct request *new_request = malloc(sizeof(struct request));
    new_request->a = a;
    new_request->b = b;
    new_request->c = c;
    new_request->num_works = num_works;
    new_request->next = NULL;

    pthread_mutex_lock(&pool->sync_mutex);
    pool->active_requests++;
    pthread_mutex_unlock(&pool->sync_mutex);
    enqueue_request(pool->request_queue, new_request);     
}

void tpool_synchronize(struct tpool *pool) {
    //fprintf(stderr, "denig269\n");

    pthread_mutex_lock(&pool->sync_mutex);
    //fprintf(stderr, "denig272\n");

    while (pool->active_requests > 0 || pool->work_queue->size > 0 || pool->request_queue->size > 0) {
        //fprintf(stderr, "denig298\n");
        if(pool != NULL){
            pthread_cond_wait(&pool->sync_cond, &pool->sync_mutex);
        }
    }
    pthread_mutex_unlock(&pool->sync_mutex);
    //fprintf(stderr, "denig278\n");

}




void tpool_destroy(struct tpool *pool) {
    pthread_mutex_lock(&pool->request_queue->mutex);
    pool->request_queue->stop = 1;
    pthread_cond_broadcast(&pool->request_queue->cond);
    pthread_mutex_unlock(&pool->request_queue->mutex);

    pthread_mutex_lock(&pool->work_queue->mutex);
    pool->work_queue->stop = 1;
    pthread_cond_broadcast(&pool->work_queue->cond);
    pthread_mutex_unlock(&pool->work_queue->mutex);

    pthread_join(pool->frontend_thread, NULL);
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->backend_threads[i], NULL);
    }

    free(pool->backend_threads);
    queue_destroy(pool->request_queue);
    queue_destroy(pool->work_queue);
    pthread_mutex_destroy(&pool->sync_mutex);
    pthread_cond_destroy(&pool->sync_cond);
    free(pool);
}

