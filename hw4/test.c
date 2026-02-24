#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct node{
    int data;
    struct node *next;
}Node;

typedef struct queue {
    void* head;             
    void* tail;            
    int size;               
} queue_t;

void enqueue(queue_t* queue, void* element, int type_size) {
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
}

void* dequeue(queue_t* queue) {

    void* element = queue->head;
    queue->head = ((void**)queue->head)[0];

    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->size--;

    return element;
}

queue_t *queue;

int main(){
    int a;
    queue = (queue_t*)malloc(sizeof(queue_t));
    for(int i = 0; i < 5; i++){
        scanf("%d", &a);
        enqueue(queue, (int*)a, sizeof(int));
    }
    while(queue->size != 0){
        int *data = (int *)dequeue(queue);
        printf("%d ", *data);
    }
    return 0;
    


}