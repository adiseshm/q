#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "t.h"

//  1.  Why use cmpxchg for SPSC - as there is only one thread for both
//      producer and consumer

// x86 Memory Consistency
//
// https://ocw.mit.edu/courses/electrical-engineering-and-computer-science/6-172-performance-engineering-of-software-systems-fall-2010/video-lectures/lecture-16-synchronizing-without-locks/MIT6_172F10_lec16.pdf
//
// 1.  Loads are not reordered with Loads
// 2.  Stores are not reordered with Stores
// 3.  Stores are not reordered with prior loads
// 4.  A load may be reordered with a prior store to a different location but not with a prior store to the same location.
// 5.  Loads and stores are not reordered with lock instructions
// 6.  Stores to the same location respect a global total order
// 7.  Lock instructions respect a global total order
// 8.  Memory ordering preserves transitive visibility (causality)
//

#define P_THR_COUNT 1
#define C_THR_COUNT 1
#define THREAD_COUNT (P_THR_COUNT + C_THR_COUNT)

#define QUEUE_SIZE 128
#define NUM_PRODUCE  ( 10 * 1000 * 1000 )
#define NUM_CONSUME  ( 10 * 1000 * 1000 )

// Let's go the easy way and keep a gap between head and tail when full.
static volatile int queue_head = 1;
static int queue[QUEUE_SIZE];
static volatile int queue_tail = 0;

static void *producer(void *data_ptr);
static void *consumer(void *data_ptr);

struct spsc_thread_t
{
   int thread_num;
   #define MAX_THREAD_NAME_LEN 128
   char thread_name[MAX_THREAD_NAME_LEN+1];
   unsigned long long elapsed_time; 
   unsigned long long num_ops; 

   struct {
      int queue_size;
      int *queue;
      int *p_queue_head;
      int *p_queue_tail;
   } q;
};

int main()
{
    pthread_t threads[THREAD_COUNT];
    struct spsc_thread_t data[THREAD_COUNT];
    int thread_id;

    for(thread_id = 0; thread_id < C_THR_COUNT; thread_id++) {
        data[thread_id].thread_num = thread_id;
        snprintf(data[thread_id].thread_name, MAX_THREAD_NAME_LEN, "c-%d", thread_id);
        data[thread_id].elapsed_time = 0;

        data[thread_id].q.queue_size = QUEUE_SIZE;
        data[thread_id].q.queue = &queue[0];

        pthread_create(&threads[thread_id], NULL, consumer, &data[thread_id]);
    }

    for(; thread_id < THREAD_COUNT; thread_id++) {
        data[thread_id].thread_num = thread_id;
        snprintf(data[thread_id].thread_name, MAX_THREAD_NAME_LEN, "p-%d", thread_id);
        data[thread_id].elapsed_time = 0;

        data[thread_id].q.queue_size = QUEUE_SIZE;
        data[thread_id].q.queue = &queue[0];

        pthread_create(&threads[thread_id], NULL, producer, &data[thread_id]);
    }

    for(thread_id = 0; thread_id < THREAD_COUNT; thread_id++) {
        pthread_join(threads[thread_id], NULL);
    }

    for(thread_id = 0; thread_id < THREAD_COUNT; thread_id++)
    {
        printf("t-name = %s (t-num %d), elapsed-time = %llu ns, num-ops %llu, per-op = %llu ns\n",
               data[thread_id].thread_name, data[thread_id].thread_num,
               data[thread_id].elapsed_time, data[thread_id].num_ops,
               data[thread_id].elapsed_time / data[thread_id].num_ops);
    }

    return 0;
}

#undef QUEUE_SIZE

static int incr_modulo_and_xchg(volatile int *p, unsigned int modulo)
{
    int old, new;
    do {
        old = *p;
        new = (old + 1) % modulo;

        // loop, if *p != old
    } while(!__sync_bool_compare_and_swap(p, old, new));

    return old;
}

static void *producer(void *data_ptr)
{
    unsigned long long start_ns = gt_ns();
    struct spsc_thread_t *p_d = (struct spsc_thread_t *)data_ptr;;

    for(int i = 0; i < NUM_PRODUCE; i++) {
        while( ( (queue_head + 1) % p_d->q.queue_size) == queue_tail) {
            // queue is full
            sleep(0);
        }

        //  store 'data' @ queue head
        p_d->q.queue[queue_head] = i;

        // move queue_head by 1, modulo QUEUE_SIZE
        incr_modulo_and_xchg(&queue_head, p_d->q.queue_size);
    }

    //  get time spent 'producing
    p_d->elapsed_time = dt_ns(start_ns);
    p_d->num_ops = NUM_PRODUCE;

    return NULL;
}

static void *consumer(void *data_ptr)
{
    unsigned long long start_ns = gt_ns();
    struct spsc_thread_t *p_d = (struct spsc_thread_t *)data_ptr;;

    for(int i = 0; i < NUM_CONSUME; i++) {
        while(queue_tail == queue_head) {
            //  queue is empty
            sleep(0);
        }
        //  move queue_tail by 1, modulo QUEUE_SIZE
        incr_modulo_and_xchg(&queue_tail, p_d->q.queue_size);
    }

    //  get time spent 'consuming
    p_d->elapsed_time = dt_ns(start_ns);
    p_d->num_ops = NUM_CONSUME;

    return NULL;
}
