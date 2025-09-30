#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "tccvdecqueue.h"

void tccvdecqueue_init(struct tccvdecqueue *q) {
    INIT_LIST_HEAD(&q->head);
    mutex_init(&q->lock);
    q->count = 0;
}

void tccvdecqueue_enqueue(struct tccvdecqueue *q, unsigned long long timestamp, int index, void *va) {
    struct tccvdecqueue_entry *entry;

    entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry) {
        pr_err("Failed to allocate memory for queue entry\n");
        return;
    }
    entry->timestamp = timestamp;
    entry->index = index;
    entry->va = va;

    mutex_lock(&q->lock);
    list_add_tail(&entry->list, &q->head);
    q->count++;
    mutex_unlock(&q->lock);

    pr_debug("Enqueued: timestamp=%llu, index=%d, va=%p, count=%d\n",
             timestamp, index, va, q->count);
}

struct tccvdecqueue_entry *tccvdecqueue_dequeue(struct tccvdecqueue *q) {
    struct tccvdecqueue_entry *entry = NULL;

    mutex_lock(&q->lock);
    if (!list_empty(&q->head)) {
        entry = list_first_entry(&q->head, struct tccvdecqueue_entry, list);
        list_del(&entry->list);
        q->count--;
    }
    mutex_unlock(&q->lock);

    if (entry) {
        pr_debug("Dequeued: timestamp=%llu, index=%d, va=%p, count=%d\n",
                 entry->timestamp, entry->index, entry->va, q->count);
    } else {
        pr_debug("Queue is empty\n");
    }
    return entry;
}

int tccvdecqueue_count(struct tccvdecqueue *q) {
    int count;
    mutex_lock(&q->lock);
    count = q->count;
    mutex_unlock(&q->lock);
    return count;
}

void tccvdecqueue_destroy(struct tccvdecqueue *q) {
    struct tccvdecqueue_entry *entry, *tmp;
    mutex_lock(&q->lock);
    list_for_each_entry_safe(entry, tmp, &q->head, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    q->count = 0;
    mutex_unlock(&q->lock);
    pr_debug("TCCVDECQueue destroyed\n");
}

void tccvdecqueue_enqueue_sorted(struct tccvdecqueue *q, unsigned long long timestamp, int index, void *va) {
    struct tccvdecqueue_entry *new_entry;
    struct list_head *pos;
    struct tccvdecqueue_entry *curr;

    new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
    if (!new_entry) {
        pr_err("Failed to allocate memory for sorted queue entry\n");
        return;
    }
    new_entry->timestamp = timestamp;
    new_entry->index = index;
    new_entry->va = va;

    mutex_lock(&q->lock);
    list_for_each(pos, &q->head) {
        curr = list_entry(pos, struct tccvdecqueue_entry, list);
        if (timestamp < curr->timestamp) {
            list_add_tail(&new_entry->list, pos);
            q->count++;
            mutex_unlock(&q->lock);
            pr_debug("Sorted Enqueued: timestamp=%llu, index=%d, va=%p, count=%d\n",
                     timestamp, index, va, q->count);
            return;
        }
    }
    list_add_tail(&new_entry->list, &q->head);
    q->count++;
    mutex_unlock(&q->lock);
    pr_debug("Sorted Enqueued at tail: timestamp=%llu, index=%d, va=%p, count=%d\n",
             timestamp, index, va, q->count);
}

void tccvdecqueue_reset(struct tccvdecqueue *q) {
    struct tccvdecqueue_entry *entry, *tmp;

    mutex_lock(&q->lock);
    list_for_each_entry_safe(entry, tmp, &q->head, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    q->count = 0;
    mutex_unlock(&q->lock);

    pr_debug("TCCVDECQueue reset: all entries cleared\n");
}

int tccvdecqueue_find(struct tccvdecqueue *q, int index) {
    struct tccvdecqueue_entry *entry;
    int found = 0;

    mutex_lock(&q->lock);
    list_for_each_entry(entry, &q->head, list) {
        if (entry->index == index) {
            found = 1;
            break;
        }
    }
    mutex_unlock(&q->lock);

    return found;
}