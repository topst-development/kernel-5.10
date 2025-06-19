#ifndef TCCVDECQUEUE_H
#define TCCVDECQUEUE_H

#include <linux/list.h>
#include <linux/mutex.h>

struct tccvdecqueue_entry {
    unsigned long long timestamp;
    int index;
    void *va;
    struct list_head list;
};

struct tccvdecqueue {
    struct list_head head;
    struct mutex lock;
    int count;
};

void tccvdecqueue_init(struct tccvdecqueue *q);
void tccvdecqueue_enqueue(struct tccvdecqueue *q, unsigned long long timestamp, int index, void *va);
struct tccvdecqueue_entry *tccvdecqueue_dequeue(struct tccvdecqueue *q);
int tccvdecqueue_count(struct tccvdecqueue *q);
void tccvdecqueue_destroy(struct tccvdecqueue *q);

void tccvdecqueue_enqueue_sorted(struct tccvdecqueue *q, unsigned long long timestamp, int index, void *va);
void tccvdecqueue_reset(struct tccvdecqueue *q);
int tccvdecqueue_find(struct tccvdecqueue *q, int index);

#endif /* TCCVDECQUEUE_H */
