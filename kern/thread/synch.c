/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
    struct semaphore *sem;

    assert(initial_count >= 0);

    sem = kmalloc(sizeof(struct semaphore));
    if (sem == NULL) {
        return NULL;
    }

    sem->name = kstrdup(namearg);
    if (sem->name == NULL) {
        kfree(sem);
        return NULL;
    }

    sem->count = initial_count;
    return sem;
}

void
sem_destroy(struct semaphore *sem)
{
    int spl;
    assert(sem != NULL);

    spl = splhigh();
    assert(thread_hassleepers(sem)==0);
    splx(spl);

    /*
     * Note: while someone could theoretically start sleeping on
     * the semaphore after the above test but before we free it,
     * if they're going to do that, they can just as easily wait
     * a bit and start sleeping on the semaphore after it's been
     * freed. Consequently, there's not a whole lot of point in
     * including the kfrees in the splhigh block, so we don't.
     */

    kfree(sem->name);
    kfree(sem);
}

void
P(struct semaphore *sem)
{
    int spl;
    assert(sem != NULL);

    /*
     * May not block in an interrupt handler.
     *
     * For robustness, always check, even if we can actually
     * complete the P without blocking.
     */
    assert(in_interrupt==0);

    spl = splhigh();
    while (sem->count==0) {
        thread_sleep(sem);
    }
    assert(sem->count>0);
    sem->count--;
    splx(spl);
}

void
V(struct semaphore *sem)
{
    int spl;
    assert(sem != NULL);
    spl = splhigh();
    sem->count++;
    assert(sem->count>0);
    thread_wakeup(sem);
    splx(spl);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
    struct lock *lock;

    lock = kmalloc(sizeof(struct lock));
    if (lock == NULL) {
        return NULL;
    }

    lock->name = kstrdup(name);
    if (lock->name == NULL) {
        kfree(lock);
        return NULL;
    }

    lock->flag = 0; // Don't lock the lock initially
    lock->holder = NULL; // No one holds the lock in the beginning
    return lock;
}

void
lock_destroy(struct lock *lock)
{
    int spl;
    assert(lock != NULL);

    // Disable interrupt, and check if there is still threads sleeping for the lock
    spl = splhigh();
    assert(thread_hassleepers(lock)==0);
    splx(spl);

    kfree(lock->name);
    kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
    // We need this to be atomic, and we only have one CPU, so we disable interrupt to acheive atomic
    int spl;
    spl = splhigh();

    while(lock->flag == 1)
        thread_sleep(lock);

    lock->flag = 1;
    lock->holder = curthread;

    splx(spl);
}

void
lock_release(struct lock *lock)
{
    // We need this to be atomic, and we only have one CPU, so we disable interrupt to acheive atomic
    int spl;
    spl = splhigh();

    lock->flag = 0;
    lock->holder = NULL;
    thread_wakeup(lock);

    splx(spl);
}

int
lock_do_i_hold(struct lock *lock)
{
    // Check if our thread is equal to the thread that hold the lock
    return (curthread == lock->holder);
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
    struct cv *cv;

    cv = kmalloc(sizeof(struct cv));
    if (cv == NULL) {
        return NULL;
    }

    cv->name = kstrdup(name);
    if (cv->name==NULL) {
        kfree(cv);
        return NULL;
    }

    // add stuff here as needed

    return cv;
}

void
cv_destroy(struct cv *cv)
{
    assert(cv != NULL);

    // add stuff here as needed

    kfree(cv->name);
    kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
    // Disable interrupt so
    int spl;
    spl = splhigh();

    lock_release(lock); // We need to release the lock so other threads can run and make this condition true
    thread_sleep(cv);
    lock_acquire(lock); // Re-acquire the lock after we wakeup

    splx(spl);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
    (void)lock; // We don't need lock here

    // Disable interrupt since it is required
    int spl;
    spl = splhigh();

    thread_wakeup(cv); // cv_signal requires us to unblock at least one thread

    splx(spl);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
    (void)lock; // We don't need lock here

    // Disable interrupt since it is required
    int spl;
    spl = splhigh();

    thread_wakeup(cv); // cv_broadcast requires us to unblock all related blocking threads

    splx(spl);
}

struct barrier *
bar_create(const char *name, int thread_count)
{
    struct barrier *bar;

    assert(thread_count >= 1);

    bar = kmalloc(sizeof(struct barrier));
    if (bar == NULL) {
        return NULL;
    }

    bar->name = kstrdup(name);
    if (bar->name == NULL) {
        kfree(bar);
        return NULL;
    }

    bar->n = thread_count;
    bar->count = 0;
    bar->mutex = sem_create("barrier_mutex", 1);
    bar->turnstile1 = sem_create("barrier_turnstile1", 0);
    bar->turnstile2 = sem_create("barrier_turnstile2", 0);
    if (bar->mutex == NULL || bar->turnstile1 == NULL || bar->turnstile2 == NULL)
        return NULL;
    return bar;
}

void
bar_wait(struct barrier *bar)
{
    int i;
    // Phase1 begin
    P(bar->mutex);
    bar->count++;
    if (bar->count == bar->n) {
        for (i = 0; i < bar->n; i++)
            V(bar->turnstile1);
    }
    V(bar->mutex);
    P(bar->turnstile1);
    // Phase1 end

    // Phase2 begin
    P(bar->mutex);
    bar->count--;
    if (bar->count == 0) {
        for (i = 0; i < bar->n; i++)
            V(bar->turnstile2);
    }
    V(bar->mutex);
    P(bar->turnstile2);
    // Phase2 end
}

void
bar_destroy(struct barrier *bar)
{
    assert(bar != NULL);

    kfree(bar->name);
    sem_destroy(bar->mutex);
    sem_destroy(bar->turnstile1);
    sem_destroy(bar->turnstile2);
    kfree(bar);
}
