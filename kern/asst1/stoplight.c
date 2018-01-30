/*
 * stoplight.c
 *
 * 31-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: You can use any synchronization primitives available to solve
 * the stoplight problem in this file.
 */


/*
 *
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>


/*
 *
 * Constants
 *
 */

/*
 * Number of cars created.
 */

#define NCARS 20

// Semaphores
static struct semaphore *thread_sem;
static struct semaphore *N_sem;
static struct semaphore *E_sem;
static struct semaphore *S_sem;
static struct semaphore *W_sem;

// Locks
static struct lock *inter_lock;
static struct lock *NE_lock;
static struct lock *NW_lock;
static struct lock *SW_lock;
static struct lock *SE_lock;

/*
 *
 * Function Definitions
 *
 */

static const char *directions[] = { "N", "E", "S", "W" };

static const char *msgs[] = {
    "approaching:",
    "region1:    ",
    "region2:    ",
    "region3:    ",
    "leaving:    "
};

/* use these constants for the first parameter of message */
enum { APPROACHING, REGION1, REGION2, REGION3, LEAVING };

static void
message(int msg_nr, int carnumber, int cardirection, int destdirection)
{
    kprintf("%s car = %2d, direction = %s, destination = %s\n",
        msgs[msg_nr], carnumber,
        directions[cardirection], directions[destdirection]);
}

/*
 * gostraight()
 *
 * Arguments:
 *  unsigned long cardirection: the direction from which the car
 *      approaches the intersection.
 *  unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *  nothing.
 *
 * Notes:
 *  This function should implement passing straight through the
 *  intersection from any direction.
 *  Write and comment this function.
 */

static
void
gostraight(unsigned long cardirection,
       unsigned long carnumber)
{
    switch (cardirection) {
    case 0: // North
        message(0, carnumber, 0, 2);

        lock_acquire(inter_lock);
        lock_acquire(NE_lock);
        lock_acquire(SE_lock);
        lock_release(inter_lock);
        V(N_sem);

        message(1, carnumber, 0, 2);

        message(2, carnumber, 0, 2);
        lock_release(NE_lock);

        message(4, carnumber, 0, 2);
        lock_release(SE_lock);
    break;

    case 1: // East
        message(0, carnumber, 1, 3);

        lock_acquire(inter_lock);
        lock_acquire(SE_lock);
        lock_acquire(SW_lock);
        lock_release(inter_lock);
        V(E_sem);

        message(1, carnumber, 1, 3);

        message(2, carnumber, 1, 3);
        lock_release(SE_lock);

        message(4, carnumber, 1, 3);
        lock_release(SW_lock);
    break;

    case 2: // South
        message(0, carnumber, 2, 0);

        lock_acquire(inter_lock);
        lock_acquire(SW_lock);
        lock_acquire(NW_lock);
        lock_release(inter_lock);
        V(S_sem);

        message(1, carnumber, 2, 0);

        message(2, carnumber, 2, 0);
        lock_release(SW_lock);

        message(4, carnumber, 2, 0);
        lock_release(NW_lock);
    break;

    case 3: // West
        message(0, carnumber, 3, 1);

        lock_acquire(inter_lock);
        lock_acquire(NW_lock);
        lock_acquire(NE_lock);
        lock_release(inter_lock);
        V(W_sem);

        message(1, carnumber, 3, 1);

        message(2, carnumber, 3, 1);
        lock_release(NW_lock);

        message(4, carnumber, 3, 1);
        lock_release(NE_lock);
    break;
    }
}


/*
 * turnleft()
 *
 * Arguments:
 *  unsigned long cardirection: the direction from which the car
 *      approaches the intersection.
 *  unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *  nothing.
 *
 * Notes:
 *  This function should implement making a left turn through the
 *  intersection from any direction.
 *  Write and comment this function.
 */

static
void
turnleft(unsigned long cardirection,
     unsigned long carnumber)
{
    /*
     * Avoid unused variable warnings.
     */

    (void) carnumber;

    switch (cardirection) {
    case 0: // North

    break;

    case 1: // East

    break;

    case 2: // South

    break;

    case 3: // West

    break;
    }

}


/*
 * turnright()
 *
 * Arguments:
 *  unsigned long cardirection: the direction from which the car
 *      approaches the intersection.
 *  unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *  nothing.
 *
 * Notes:
 *  This function should implement making a right turn through the
 *  intersection from any direction.
 *  Write and comment this function.
 */

static
void
turnright(unsigned long cardirection,
      unsigned long carnumber)
{
    /*
     * Avoid unused variable warnings.
     */

    (void) carnumber;

    switch (cardirection) {
    case 0: // North

    break;

    case 1: // East

    break;

    case 2: // South

    break;

    case 3: // West

    break;
    }
}


/*
 * approachintersection()
 *
 * Arguments:
 *  void * unusedpointer: currently unused.
 *  unsigned long carnumber: holds car id number.
 *
 * Returns:
 *  nothing.
 *
 * Notes:
 *  Change this function as necessary to implement your solution. These
 *  threads are created by createcars().  Each one must choose a direction
 *  randomly, approach the intersection, choose a turn randomly, and then
 *  complete that turn.  The code to choose a direction randomly is
 *  provided, the rest is left to you to implement.  Making a turn
 *  or going straight should be done by calling one of the functions
 *  above.
 */

static
void
approachintersection(void * unusedpointer,
             unsigned long carnumber)
{
    int cardirection;
    int carheading;

    /*
     * Avoid unused variable and function warnings.
     */

    (void) unusedpointer;

    void (*car_heading_ptrs[])(unsigned long, unsigned long) = {gostraight, turnleft, turnright};

    /*
     * cardirection is set randomly.
     */

    cardirection = random() % 4;
    //cardirection = 2;
    //carheading = random() % 3;
    carheading = 0;

    switch (cardirection) {
    case 0: // North
        P(N_sem);
    break;
    case 1: // East
        P(E_sem);
    break;
    case 2: // South
        P(S_sem);
    break;
    case 3: // West
        P(W_sem);
    break;
    }

    car_heading_ptrs[carheading](cardirection, carnumber);

    V(thread_sem);
}


/*
 * createcars()
 *
 * Arguments:
 *  int nargs: unused.
 *  char ** args: unused.
 *
 * Returns:
 *  0 on success.
 *
 * Notes:
 *  Driver code to start up the approachintersection() threads.  You are
 *  free to modiy this code as necessary for your solution.
 */

int
createcars(int nargs,
       char ** args)
{
    int index, error;

    /*
     * Avoid unused variable warnings.
     */

    (void) nargs;
    (void) args;

    thread_sem = sem_create("Thread Join", 0);
    if (thread_sem == NULL)
        return 1;
    N_sem = sem_create("N", 1);
    if (N_sem == NULL)
        return 1;
    E_sem = sem_create("E", 1);
    if (E_sem == NULL)
        return 1;
    S_sem = sem_create("S", 1);
    if (S_sem == NULL)
        return 1;
    W_sem = sem_create("W", 1);
    if (W_sem == NULL)
        return 1;

    inter_lock = lock_create("Intersection Lock");
    if (inter_lock == NULL)
        return 1;
    NE_lock = lock_create("NE");
    if (NE_lock == NULL)
        return 1;
    NW_lock = lock_create("NW");
    if (NW_lock == NULL)
        return 1;
    SW_lock = lock_create("SW");
    if (SW_lock == NULL)
        return 1;
    SE_lock = lock_create("SE");
    if (SE_lock == NULL)
        return 1;

    /*
     * Start NCARS approachintersection() threads.
     */

    for (index = 0; index < NCARS; index++) {

        error = thread_fork("approachintersection thread",
                    NULL,
                    index,
                    approachintersection,
                    NULL
                    );

        /*
         * panic() on error.
         */

        if (error) {

            panic("approachintersection: thread_fork failed: %s\n",
                  strerror(error)
                  );
        }
    }

    // Only destroy things when all thread finish
    for (index = 0; index < NCARS; index++)
        P(thread_sem);
    sem_destroy(thread_sem);
    sem_destroy(N_sem);
    sem_destroy(E_sem);
    sem_destroy(S_sem);
    sem_destroy(W_sem);
    lock_destroy(inter_lock);
    lock_destroy(NE_lock);
    lock_destroy(NW_lock);
    lock_destroy(SW_lock);
    lock_destroy(SE_lock);

    return 0;
}
