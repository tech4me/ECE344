/*
 * catsem.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: Please use SEMAPHORES to solve the cat syncronization problem in
 * this file.
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
 * Number of food bowls.
 */

#define NFOODBOWLS 2

/*
 * Number of cats.
 */

#define NCATS 6

/*
 * Number of mice.
 */

#define NMICE 2

/*
 * Number of iteration we run.
 */

#define NITERATION 4

enum food_state {
    S_NONE,
    S_CAT,
    S_MOUSE
};

// The structure and keeps track of the current state of the food
static unsigned int food_state[NFOODBOWLS];

// Semaphores
static struct semaphore *thread_sem;
static struct semaphore *food_state_sem;

// Barrier
static struct barrier *iteration_bar;

// Iteration counters
static unsigned int cats_iteration_count[NCATS];
static unsigned int mice_iteration_count[NMICE];

/*
 *
 * Function Definitions
 *
 */

/* who should be "cat" or "mouse" */
static void
sem_eat(const char *who, int num, int bowl, int iteration)
{
    kprintf("%s: %d starts eating: bowl %d, iteration %d\n", who, num,
        bowl, iteration);
    clocksleep(1);
    kprintf("%s: %d ends eating: bowl %d, iteration %d\n", who, num,
        bowl, iteration);
}

/*
 * catsem()
 *
 * Arguments:
 *  void * unusedpointer: currently unused.
 *  unsigned long catnumber: holds the cat identifier from 0 to NCATS - 1.
 *
 * Returns:
 *  nothing.
 *
 * Notes:
 *  Write and comment this function using semaphores.
 *
 */

static
void
catsem(void * unusedpointer,
       unsigned long catnumber)
{
    (void) unusedpointer;

    while (cats_iteration_count[catnumber] < NITERATION) {
        int i, bowl_num, flag;
        P(food_state_sem);
        for (i = 0; i < NFOODBOWLS; i++) {
            if (food_state[i] == S_NONE) {
                flag = 0;
                bowl_num = i;
                int j;
                for (j = i + 1; j < NFOODBOWLS; j++) {
                    // Make sure we have no mouse on the other bowl
                    if (food_state[j] == S_MOUSE) {
                        flag = 1;
                        break;
                    }
                }
                break;
            } else if (food_state[i] == S_MOUSE) {
                // We have a mouse also eating, so we should wait until the cat is gone
                flag = 1;
                break;
            } else {
                // No available bowl
                flag = 2;
            }
        }
        if (!flag) {
            food_state[i] = S_CAT;
            V(food_state_sem);
            sem_eat("cat", catnumber, i + 1, cats_iteration_count[catnumber]++);
            P(food_state_sem);
            food_state[i] = S_NONE;
            V(food_state_sem);
            bar_wait(iteration_bar);
        } else {
            V(food_state_sem);
        }
    }
    // Semaphore + 1 when thread finish here
    V(thread_sem);
}


/*
 * mousesem()
 *
 * Arguments:
 *  void * unusedpointer: currently unused.
 *  unsigned long mousenumber: holds the mouse identifier from 0 to
 *      NMICE - 1.
 *
 * Returns:
 *  nothing.
 *
 * Notes:
 *  Write and comment this function using semaphores.
 *
 */

static
void
mousesem(void * unusedpointer,
     unsigned long mousenumber)
{
    (void) unusedpointer;

    while (mice_iteration_count[mousenumber] < NITERATION) {
        int i, bowl_num, flag;
        P(food_state_sem);
        for (i = 0; i < NFOODBOWLS; i++) {
            if (food_state[i] == S_NONE) {
                flag = 0;
                bowl_num = i;
                int j;
                for (j = i + 1; j < NFOODBOWLS; j++) {
                    // Make sure we have no cat on the other bowl
                    if (food_state[j] == S_CAT) {
                        flag = 1;
                        break;
                    }
                }
                break;
            } else if (food_state[i] == S_CAT) {
                // We have a cat also eating, so we should wait until the cat is gone
                flag = 1;
                break;
            } else {
                // No available bowl
                flag = 2;
            }
        }
        if (!flag) {
            food_state[bowl_num] = S_MOUSE;
            V(food_state_sem);
            sem_eat("mouse", mousenumber, bowl_num + 1, mice_iteration_count[mousenumber]++);
            P(food_state_sem);
            food_state[i] = S_NONE;
            V(food_state_sem);
            bar_wait(iteration_bar);
        } else {
            V(food_state_sem);
        }
    }

    // Semaphore + 1 when thread finish here
    V(thread_sem);
}


/*
 * catmousesem()
 *
 * Arguments:
 *  int nargs: unused.
 *  char ** args: unused.
 *
 * Returns:
 *  0 on success.
 *
 * Notes:
 *  Driver code to start up catsem() and mousesem() threads.  Change this
 *  code as necessary for your solution.
 */

int
catmousesem(int nargs,
        char ** args)
{
    int index, error;

    /*
     * Avoid unused variable warnings.
     */

    (void) nargs;
    (void) args;
    for(index = 0; index < NFOODBOWLS; index++)
        food_state[index] = 0;
    for(index = 0; index < NCATS; index++)
        cats_iteration_count[index] = 0;
    for(index = 0; index < NMICE; index++)
        mice_iteration_count[index] = 0;

    thread_sem = sem_create("Thread Join", 0);
    if (thread_sem == NULL)
        return 1;

    food_state_sem = sem_create("Food State", 1);
    if (food_state_sem == NULL)
        return 1;

    iteration_bar = bar_create("Iteration Sync", NCATS + NMICE);
    if (iteration_bar == NULL)
        return 1;

    /*
     * Start NCATS catsem() threads.
     */

    for (index = 0; index < NCATS; index++) {

        error = thread_fork("catsem Thread",
                    NULL,
                    index,
                    catsem,
                    NULL
                    );

        /*
         * panic() on error.
         */

        if (error) {

            panic("catsem: thread_fork failed: %s\n",
                  strerror(error)
                  );
        }
    }

    /*
     * Start NMICE mousesem() threads.
     */

    for (index = 0; index < NMICE; index++) {

        error = thread_fork("mousesem Thread",
                    NULL,
                    index,
                    mousesem,
                    NULL
                    );

        /*
         * panic() on error.
         */

        if (error) {

            panic("mousesem: thread_fork failed: %s\n",
                  strerror(error)
                  );
        }
    }

    // Only destory semaphores when all threads finish their jobs
    for (index = 0; index < (NCATS + NMICE); index++)
        P(thread_sem);
    sem_destroy(food_state_sem);
    sem_destroy(thread_sem);
    bar_destroy(iteration_bar);

    return 0;
}


/*
 * End of catsem.c
 */
