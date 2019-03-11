#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>

#define SUCCESS 0

#define RANDOM_LIM 100
#define SUBMATRIX_LIM 5
#define NUM_OF_SUBMATRIX (N/5) * (N/5)

// Struct containing info about numbers of each threads
typedef struct {
    int generate_thread_num;
    int log_thread_num;
    int mod_thread_num;
    int add_thread_num;
} THREAD_INFO;

// Node struct for queue #1 - created by generate thread
typedef struct sq {
    pthread_t tid;
    int order;
    int submatrix_order;
    int** submatrix;
    int checkers;
    struct sq* next;
} SUBMATRIX_QUEUE;

// Node struct for queue #2 - created by mod thread
typedef struct modq {
    pthread_t tid;
    int order;
    int submatrix_order;
    int** submatrix;
    struct modq* next;
} MOD_QUEUE;

// Struct for passing parameter to thread routine of generate thread
typedef struct {
    pthread_t tid;
    int order;
} GENERATE_THREAD_PARAM;

// Struct for returning values inside of queue #1 node
typedef struct {
    pthread_t tid;
    int order;
    int submatrix_order;
    int** submatrix;
} SQ_PARAM;

/* GLOBAL VARIABLES */

ssize_t N = 0;                      // size of big matrix


int log_counter = 0;                // counts number of logged sub matrices
int mod_counter = 0;                // counts number of modded sub matrices
int add_counter = 0;                // counts number of added sub matrices
int current_sub_matrix = 0;         // counts number of generated sub matrices
int current_mod_matrix = 0;         // counts number of generated sub matrices by mod threads

int global_sum = 0;                 // Global Sum variable for add threads

/* Required mutex and semaphore variables - Described in project report */
pthread_mutex_t generate_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mod_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t add_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t checker_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t empty_generate_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t empty_mod_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t printf_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t empty_generate_queue = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty_mod_queue = PTHREAD_COND_INITIALIZER;
pthread_cond_t dequeue_cond = PTHREAD_COND_INITIALIZER;

sem_t peek_semaphore;

// Set main matrix as NULL
int** MAIN_MATRIX = NULL;

// Set queue #1 and queue #2 as NULL
SUBMATRIX_QUEUE* submatrix_queue = NULL;
MOD_QUEUE* mod_queue = NULL;

// Pointer that contains information about number of threads
THREAD_INFO* info = NULL;

/* GLOBAL VARIABLES */


/* FUNCTION PROTOTYPES */

void init_args(int argc, char* argv[]);        // parses arguments given by user
int check_N(void);                             // checks main matrix size

// Queue utility functions for queue #1 (created by generate thread)
void enqueue_generate(int** submatrix, pthread_t tid, int order, int submatrix_order);
void dequeue_generate();
void peek_generate_queue(const char* caller, SQ_PARAM** param);

// Queue utility functions for queue #2 (created by mod thread)
void enqueue_mod(int** submatrix, pthread_t tid, int order, int submatrix_order);
void dequeue_mod(int*** submatrix, pthread_t* tid, int* order, int* submatrix_order);

void create_threads(void);                  // Function responsible for creating and joining all threads

// Thread routines
void* generate_submatrix(void* param);      // Routine for generate thread
void* log_submatrix(void* param);           // Routine for log thread
void* mod_submatrix(void* param);           // Routine for mod thread
void* add_submatrix(void* param);           // Routine for add thread

void allocate_main_matrix(void);                    // Allocate required space for main matrix
int** allocate_mod_matrix(void);                    // Allocate 5x5 matrix for mod thread
void init_submatrix(int** source, int*** dest);     // Copy contents of a sub matrix to another subm atrix

void print_submatrix(int** submatrix);              // Prints sub matrix properly
void print_output(const char* file_name);           // Prints main matrix and global sum to a text file

/* FUNCTION PROTOTYPES */

int main(int argc, char* argv[]) {

    srand((unsigned int)time(NULL));                // Set random seed

    sem_init(&peek_semaphore, 0, 2);                // Initialize semaphore, this semaphore accepts two threads inside

    printf("Welcome to the matrix creating program!\n");

    init_args(argc, argv);                          // Initialize and check arguments

    create_threads();                               // Creates and joins all threads

    print_output("out.txt");                        // Print global sum and big matrix to a text file

    printf("Global sum: %d", global_sum);           // Also print global sum

    exit(0);
}

/* Enqueue function for the queue #1 */
void enqueue_generate(int** submatrix, pthread_t tid, int order, int submatrix_order) {

    // If queue is empty
    if(submatrix_queue == NULL) {
        submatrix_queue = (SUBMATRIX_QUEUE*)malloc(sizeof(SUBMATRIX_QUEUE));
        submatrix_queue->submatrix = submatrix;
        submatrix_queue->tid = tid;
        submatrix_queue->order = order;
        submatrix_queue->submatrix_order = submatrix_order;
        submatrix_queue->checkers = 0;
        submatrix_queue->next = NULL;
        // Also send a signal that this queue is not empty anymore
        pthread_cond_signal(&empty_generate_queue);
        return;
    }

    SUBMATRIX_QUEUE* iter = submatrix_queue;

    // Find the space
    while(iter->next != NULL)
        iter = iter->next;

    // Add new 5x5 submatrix to the queue
    SUBMATRIX_QUEUE* new_submatrix = (SUBMATRIX_QUEUE*)malloc(sizeof(SUBMATRIX_QUEUE));

    new_submatrix->submatrix = submatrix;
    new_submatrix->tid = tid;
    new_submatrix->order = order;
    new_submatrix->submatrix_order = submatrix_order;
    new_submatrix->checkers = 0;
    new_submatrix->next = NULL;
    iter->next = new_submatrix;
}

/* Dequeue function for the queue #1 */
void dequeue_generate() {

    // Remove tail from the queue
    SUBMATRIX_QUEUE *dequeued = submatrix_queue;
    submatrix_queue = submatrix_queue->next;

    dequeued->next = NULL;
    free(dequeued);
    pthread_cond_broadcast(&dequeue_cond);
}

/* Peek function for the queue #1 */
void peek_generate_queue(const char* caller, SQ_PARAM** param) {

    // Accept two threads and these will be one mod thread and one log thread
    sem_wait(&peek_semaphore);

    // Let both threads read the data inside head of the queue concurrently
    SQ_PARAM* ret_param = (SQ_PARAM*)malloc(sizeof(SQ_PARAM));
    init_submatrix(submatrix_queue->submatrix, &(ret_param->submatrix));
    ret_param->tid = submatrix_queue->tid;
    ret_param->order = submatrix_queue->order;
    ret_param->submatrix_order = submatrix_queue->submatrix_order;

    *param = ret_param;

    // Increment checkers field of the of queue
    pthread_mutex_lock(&checker_mutex);
    submatrix_queue->checkers++;

    // If two threads checked and red data from the head
    if(submatrix_queue->checkers == 2) {
        // Print info
        pthread_mutex_lock(&printf_mutex);
        printf("%s thread red from queue. dequeueing...\n", caller);
        pthread_mutex_unlock(&printf_mutex);
        // And current head is ready to be dequeued
        dequeue_generate();
    }
    // If only one thread red from the queue, wait for other one to read
    else {
        // Print info
        pthread_mutex_lock(&printf_mutex);
        printf("%s thread red from queue. waiting for dequeue...\n", caller);
        pthread_mutex_unlock(&printf_mutex);
        // Wait until other thread read head
        pthread_cond_wait(&dequeue_cond, &checker_mutex);
        // After other thread reads, it'll dequeue
        // And we can continue...
    }
    pthread_mutex_unlock(&checker_mutex);

    sem_post(&peek_semaphore);
}

/* Enqueue function for the queue #2 */
void enqueue_mod(int** submatrix, pthread_t tid, int order, int submatrix_order) {

    // If queue is empty
    if(mod_queue == NULL) {
        mod_queue = (MOD_QUEUE*)malloc(sizeof(MOD_QUEUE));
        init_submatrix(submatrix, &(mod_queue->submatrix));
        mod_queue->tid = tid;
        mod_queue->order = order;
        mod_queue->submatrix_order = submatrix_order;
        mod_queue->next = NULL;
        // Signal that this queue is not empty anymore
        pthread_cond_signal(&empty_mod_queue);
        return;
    }

    MOD_QUEUE* iter = mod_queue;

    // Find the space
    while(iter->next != NULL)
        iter = iter->next;

    // Add new 5x5 modded submatrix to the queue
    MOD_QUEUE* new_submatrix = (MOD_QUEUE*)malloc(sizeof(MOD_QUEUE));

    init_submatrix(submatrix, &(new_submatrix->submatrix));
    new_submatrix->tid = tid;
    new_submatrix->order = order;
    new_submatrix->submatrix_order = submatrix_order;
    new_submatrix->next = NULL;
    iter->next = new_submatrix;
}

/* Dequeue function for the queue #2 */
void dequeue_mod(int*** submatrix, pthread_t* tid, int* order, int* submatrix_order) {

    // If queue is empty
    if(mod_queue == NULL) {
        fprintf(stderr, "Mod queue is empty!\n");
        return;
    }

    // Initialize data stored in head
    *(submatrix) = mod_queue->submatrix;
    *(tid) = mod_queue->tid;
    *(order) = mod_queue->order;
    *(submatrix_order) = mod_queue->submatrix_order;

    // Since only one add thread should reach to head of the queue, deque directly (without waiting any other thread)
    MOD_QUEUE *dequeued = mod_queue;
    mod_queue = mod_queue->next;

    dequeued->next = NULL;
    free(dequeued);
}

/* Parse and initialize arguments given by user */
void init_args(int argc, char* argv[]) {

    int generate_thread_num,
        log_thread_num,
        mod_thread_num,
        add_thread_num;

    int thread_nums[4];

    int opt;
    int counter = 0;

    // Options are -d and -n
    while ((opt = getopt(argc, argv, "d:n:")) != -1) {
        switch(opt) {
            // size of main matrix
            case 'd':                   // if -d
                N = (ssize_t)strtol(optarg, NULL, 10);
                break;
            case 'n':                   // if -n
                optind--;
                for( ;optind < argc && *argv[optind] != '-'; optind++) {
                    thread_nums[counter] = (int)strtol(argv[optind], NULL, 10);
                    if(thread_nums[counter] <= 0) {
                        fprintf(stderr, "num of threads must be greater than 0!\n");
                        exit(-1);
                    }
                    counter++;
                }
                if(counter < 4) {
                    fprintf(stderr, "-n option exactly requires 4 positive integers!\n");
                    exit(-1);
                }
                break;
            case '?':
                fprintf(stderr, "unknown option %c!\n", optopt);
                exit(-1);
            default:
                fprintf(stderr, "unknown option %c!\n", optopt);
                exit(-1);
        }
    }

    // check N value
    check_N();

    generate_thread_num = thread_nums[0];
    log_thread_num = thread_nums[1];
    mod_thread_num = thread_nums[2];
    add_thread_num = thread_nums[3];

    printf("Size of main matrix: %zu\n", N);
    printf("Number of generate threads: %d\n", generate_thread_num);
    printf("Number of log threads: %d\n", log_thread_num);
    printf("Number of mod threads: %d\n", mod_thread_num);
    printf("Number of add threads: %d\n", add_thread_num);

    // Init thread information
    info = (THREAD_INFO*)malloc(sizeof(THREAD_INFO));

    info->generate_thread_num = generate_thread_num;
    info->log_thread_num = log_thread_num;
    info->mod_thread_num = mod_thread_num;
    info->add_thread_num = add_thread_num;

}

/* Check size of the big matrix */
int check_N() {

    if(N <= 0) {
        fprintf(stderr, "please give an integer for N that is greater than 0\n");
        exit(-1);
    }

    if(N % 5 != 0) {
        fprintf(stderr, "please give an integer for N that is a multiply of 5\n");
        exit(-1);
    }

    return SUCCESS;

}

/* Creator of all threads */
void create_threads() {

    long generate_thread_num = (long)info->generate_thread_num;
    long log_thread_num = (long)info->log_thread_num;
    long mod_thread_num = (long)info->mod_thread_num;
    long add_thread_num = (long)info->add_thread_num;

    pthread_t generate_threads[generate_thread_num];
    pthread_t log_threads[log_thread_num];
    pthread_t mod_threads[mod_thread_num];
    pthread_t add_threads[add_thread_num];
    int rc;
    long t;
    void *status;

    // create generate threads
    for(t=0; t<generate_thread_num; t++) {

        GENERATE_THREAD_PARAM* param = (GENERATE_THREAD_PARAM*)malloc(sizeof(GENERATE_THREAD_PARAM));
        param->order = (int)t;

        rc = pthread_create(&generate_threads[t], NULL, generate_submatrix, (void*)param);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    // create log threads
    for(t=0; t<log_thread_num; t++) {

        int order = (int)t;

        rc = pthread_create(&log_threads[t], NULL, log_submatrix, (void*)order);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    // create mod threads
    for(t=0; t<mod_thread_num; t++) {

        int param = (int)t;

        rc = pthread_create(&mod_threads[t], NULL, mod_submatrix, (void*)param);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    // create add threads
    for(t=0; t<add_thread_num; t++) {

        int param = (int)t;

        rc = pthread_create(&add_threads[t], NULL, add_submatrix, (void*)param);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    // wait for generate threads to terminate
    for(t=0; t<generate_thread_num; t++) {

        rc = pthread_join(generate_threads[t], &status);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc);
            exit(-1);
        }
    }

    // wait for log threads to terminate
    for(t=0; t<log_thread_num; t++) {

        rc = pthread_join(log_threads[t], &status);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc);
            exit(-1);
        }
    }

    // wait for mod threads to terminate
    for(t=0; t<mod_thread_num; t++) {

        rc = pthread_join(mod_threads[t], &status);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc);
            exit(-1);
        }
    }

    // wait for add threads to terminate
    for(t=0; t<add_thread_num; t++) {

        rc = pthread_join(add_threads[t], &status);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc);
            exit(-1);
        }
    }

    // Destroy all mutexes, condition variables and semaphores.
    pthread_mutex_destroy(&generate_mutex);
    pthread_mutex_destroy(&log_mutex);
    pthread_mutex_destroy(&mod_mutex);
    pthread_mutex_destroy(&add_mutex);
    pthread_mutex_destroy(&checker_mutex);
    pthread_mutex_destroy(&printf_mutex);
    pthread_mutex_destroy(&empty_generate_mutex);
    pthread_mutex_destroy(&empty_mod_mutex);

    pthread_cond_destroy(&empty_generate_queue);
    pthread_cond_destroy(&empty_mod_queue);
    pthread_cond_destroy(&dequeue_cond);

    sem_destroy(&peek_semaphore);
}

/* Routine for generate threads */
void* generate_submatrix(void* param) {

    int i, j;

    // Create sub matrices until reaching enough number of sub matrices
    while(1) {

        int **submatrix = (int **) malloc(sizeof(int *) * SUBMATRIX_LIM);

        for (i = 0; i < SUBMATRIX_LIM; i++) {

            submatrix[i] = (int *) malloc(sizeof(int) * SUBMATRIX_LIM);

            for (j = 0; j < SUBMATRIX_LIM; j++)
                submatrix[i][j] = (rand() % RANDOM_LIM) + 1;    // Generate random numbers between 1 and 100.

        }

        ((GENERATE_THREAD_PARAM *) param)->tid = pthread_self();

        // After creating a random 5x5 matrix, check number of created matrices
        // Acquire mutex since we are going to update some global variables
        pthread_mutex_lock(&generate_mutex);

        // If there are more matrix to create
        if(current_sub_matrix < NUM_OF_SUBMATRIX) {
            pthread_mutex_lock(&empty_generate_mutex);
            // Enqueue created submatrix to the queue #1
            enqueue_generate(submatrix, ((GENERATE_THREAD_PARAM *)param)->tid,
                    ((GENERATE_THREAD_PARAM *)param)->order, current_sub_matrix);
            pthread_mutex_unlock(&empty_generate_mutex);
            // Calculate current subrow and subcolumn
            int sub_row = current_sub_matrix / (int)(N / SUBMATRIX_LIM);
            int sub_col = current_sub_matrix % (int)(N / SUBMATRIX_LIM);

            // Then print information
            pthread_mutex_lock(&printf_mutex);
            printf("Generate Thread %d created matrix [%d, %d]: \n", ((GENERATE_THREAD_PARAM *)param)->order,
                                                                                                        sub_row,
                                                                                                        sub_col);
            print_submatrix(submatrix);
            pthread_mutex_unlock(&printf_mutex);

            // Create the next sub matrix
            current_sub_matrix++;

        }
        // If number of sub matrices are enough, release mutex and exit this thread
        else {
            pthread_mutex_unlock(&generate_mutex);
            break;
        }
        pthread_mutex_unlock(&generate_mutex);
    }

    pthread_exit(NULL);
}

/* Routine for log threads */
void* log_submatrix(void* param) {
    // Order of thread given as a parameter
    int thread_order = (int)param;

    // Acquire lock since we are manipulating a global variable
    pthread_mutex_lock(&log_mutex);
    // Thread that arrives first will allocate the main matrix
    if(MAIN_MATRIX == NULL)
        allocate_main_matrix();
    pthread_mutex_unlock(&log_mutex);

    // Log until reach a specified number of sub matrices
    while(1) {

        int row, col;
        int i, j, k = 0, l = 0;

        // Acquire lock since we are going to update a global variable
        pthread_mutex_lock(&log_mutex);

        // If all sub matrices are logged, release lock and exit thread
        if(log_counter == NUM_OF_SUBMATRIX) {
            pthread_mutex_unlock(&log_mutex);
            break;
        }

        // If queue #1 is empty, check will there be any enqueue operation or not
        while(submatrix_queue == NULL) {
            // If there will be an enqueue operation, wait until operation is done
            if(current_sub_matrix < NUM_OF_SUBMATRIX) {
                pthread_cond_wait(&empty_generate_queue, &empty_generate_mutex);
            }
            // If there won't be an enqueue, exit the thread
            else {
                pthread_mutex_unlock(&log_mutex);
                pthread_exit(NULL);
            }
        }

        int** submatrix;
        int submatrix_order;

        SQ_PARAM* ret_param = NULL;
        char caller_string[64];
        sprintf(caller_string, "Log Thread %d", thread_order);
        // Read head of the thread and wait until deque operation is done
        peek_generate_queue(caller_string, &ret_param);

        // Increment log counter
        log_counter++;

        // Unpack required parameters coming from head of the queue
        submatrix = ret_param->submatrix;
        submatrix_order = ret_param->submatrix_order;

        // Now we can release the lock
        pthread_mutex_unlock(&log_mutex);

        // Calculate current sub row and sub column
        row = submatrix_order / (int)(N / SUBMATRIX_LIM);
        col = submatrix_order % (int)(N / SUBMATRIX_LIM);

        // Print the job that done by thread
        pthread_mutex_lock(&printf_mutex);
        printf("Log Thread %d append this submatrix to [%d, %d]:\n", thread_order, row, col);
        print_submatrix(submatrix);
        pthread_mutex_unlock(&printf_mutex);

        // Append submatrix to the correct location
        for(i=(SUBMATRIX_LIM*row); i<(SUBMATRIX_LIM*row)+SUBMATRIX_LIM; i++) {
            for(j=(SUBMATRIX_LIM*col); j<(SUBMATRIX_LIM*col)+SUBMATRIX_LIM; j++) {
                MAIN_MATRIX[i][j] = submatrix[k][l];
                l++;
            }
            l = 0;
            k++;
        }
    }
    pthread_exit(NULL);
}

/* Routine for mod threads */
void* mod_submatrix(void* param) {

    int thread_order = (int)param;

    // Mod until mod counter reaches it's limit
    while(1) {

        int row, col;
        int i, j;
        int mod_base;

        // Acquire lock, we are going to update a global variable
        pthread_mutex_lock(&mod_mutex);

        // If enough mod operations are done, release the lock and exit thread
        if(mod_counter == NUM_OF_SUBMATRIX) {
            pthread_mutex_unlock(&mod_mutex);
            break;
        }

        // If queue #1 is empty, check will there be any enqueue or not
        while(submatrix_queue == NULL) {
            // If there will be an enqueue
            if(current_sub_matrix < NUM_OF_SUBMATRIX) {
                // Wait for it to be done
                pthread_cond_wait(&empty_generate_queue, &empty_generate_mutex);
            }
            // If there won't be any enqueue operation, exit the thread
            else {
                pthread_mutex_unlock(&mod_mutex);
                pthread_exit(NULL);
            }
        }

        int** submatrix;
        int** mod_matrix;
        pthread_t tid;
        int order;
        int submatrix_order;

        SQ_PARAM* ret_param = NULL;
        char caller_string[64];
        sprintf(caller_string, "Mod Thread %d", thread_order);
        // Read data from head of the queue, also wait for dequeue inside of this function
        peek_generate_queue(caller_string, &ret_param);

        // Increment mod counter since we read the data for current mod operation
        mod_counter++;

        // Unpack required parameters coming from head of the queue
        submatrix = ret_param->submatrix;
        tid = ret_param->tid;
        order = ret_param->order;
        submatrix_order = ret_param->submatrix_order;

        // Release the lock
        pthread_mutex_unlock(&mod_mutex);

        // Get base for modulo operation
        mod_base = submatrix[0][0];

        // Allocate 5x5 matrix for mod operation
        mod_matrix = allocate_mod_matrix();

        // Fill 5x5 matrix with calculated values
        for(i=0; i<SUBMATRIX_LIM; i++)
            for(j=0; j<SUBMATRIX_LIM; j++)
                mod_matrix[i][j] = submatrix[i][j] % mod_base;

        // Calculate sub row and sub column
        row = submatrix_order / (int)(N / SUBMATRIX_LIM);
        col = submatrix_order % (int)(N / SUBMATRIX_LIM);

        // Acquire lock again, we'll enqueue to the queue #2 with modded matrices
        pthread_mutex_lock(&mod_mutex);
        // If we do not enqued all of the modded sub matrices, enqueue
        if(current_mod_matrix < NUM_OF_SUBMATRIX) {
            pthread_mutex_lock(&empty_mod_mutex);
            enqueue_mod(mod_matrix, tid, thread_order, submatrix_order);        // Enqueue
            pthread_mutex_unlock(&empty_mod_mutex);

            current_mod_matrix++;                      // And we can update counter for queue #2 since we enqueued
        }
        // If we enqued all modded sub matrices, release lock and exit the thread
        else {
            pthread_mutex_unlock(&mod_mutex);
            break;
        }
        pthread_mutex_unlock(&mod_mutex);

        // Print information about thread
        pthread_mutex_lock(&printf_mutex);
        printf("Mod Thread %d created this matrix from submatrix [%d, %d]:\n", thread_order, row, col);
        print_submatrix(mod_matrix);
        pthread_mutex_unlock(&printf_mutex);
    }

    pthread_exit(NULL);
}

/* Routine for add threads */
void* add_submatrix(void* param) {

    int thread_order = (int)param;

    // Iterate until all add operations are done
    while(1) {
        int local_sum = 0, prev_sum;
        int row, col;
        int i, j;

        // Acquire lock since we are going to change a global variable
        pthread_mutex_lock(&add_mutex);

        // If we added all modded submatrices, release the lock and exit the thread
        if(add_counter == NUM_OF_SUBMATRIX) {
            pthread_mutex_unlock(&add_mutex);
            break;
        }

        // If queue #2 is NULL, check if there will be any enqueue operation or not
        while(mod_queue == NULL) {
            // If there will be some enqueue operation, wait for it to be done
            if(current_mod_matrix < NUM_OF_SUBMATRIX) {
                pthread_cond_wait(&empty_mod_queue, &empty_mod_mutex);
            }
            // If all modded sub matrices are enqued, release the lock and exit the thread
            else {
                pthread_mutex_unlock(&add_mutex);
                pthread_exit(NULL);
            }
        }

        int** submatrix;
        pthread_t tid;
        int order;
        int submatrix_order;

        // Dequeue and initialize data that head of the queue #2 contains
        dequeue_mod(&submatrix, &tid, &order, &submatrix_order);
        // Increment add counter since we read the data and dequeue
        add_counter++;

        pthread_mutex_unlock(&add_mutex);

        // Compute local sum outside of lock because it's not a shared variable
        for(i=0; i<SUBMATRIX_LIM; i++) {
            for (j=0; j<SUBMATRIX_LIM; j++) {
                local_sum += submatrix[i][j];
            }
        }

        // Acquire lock since we are updating global sum value
        pthread_mutex_lock(&add_mutex);
        prev_sum = global_sum;
        global_sum += local_sum;
        pthread_mutex_unlock(&add_mutex);

        // Calculate current sub row and sub column
        row = submatrix_order / (int)(N / SUBMATRIX_LIM);
        col = submatrix_order % (int)(N / SUBMATRIX_LIM);

        // Print information about job of the thread
        pthread_mutex_lock(&printf_mutex);
        printf("Add Thread %d has local sum %d by [%d, %d] submatrix.", thread_order, local_sum, row, col);
        printf(" Global sum before/after update: %d/%d.\n", prev_sum, global_sum);
        pthread_mutex_unlock(&printf_mutex);
    }

    pthread_exit(NULL);
}

/* Allocate required size for big matrix */
void allocate_main_matrix() {
    int i;

    MAIN_MATRIX = (int**)calloc((size_t)N, sizeof(int*));

    for(i=0; i<N; i++)
        MAIN_MATRIX[i] = (int*)calloc((size_t)N, sizeof(int));

}

/* Allocate a 5x5 matrix for mod threads */
int** allocate_mod_matrix() {
    int i;

    int** matrix = (int**)malloc(SUBMATRIX_LIM * sizeof(int*));

    for(i=0; i<SUBMATRIX_LIM; i++)
        matrix[i] = (int*)malloc(SUBMATRIX_LIM * sizeof(int));

    return matrix;
}

/* Print 5x5 submatrix properly */
void print_submatrix(int** submatrix) {
    int i, j;
    printf("[");
    for(i=0; i<SUBMATRIX_LIM; i++) {
        if(i == 0)
            printf("[  ");
        else
            printf("\n [  ");
        for(j=0; j<SUBMATRIX_LIM; j++)
            printf("%-4d", submatrix[i][j]);

        printf("]");
    }
    printf("]\n");
}

/* Print big matrix and global sum to a text file */
void print_output(const char* file_name) {

    FILE* out;

    out = fopen(file_name, "w");

    int i, j;
    fprintf(out, "The big matrix is:\n[");
    for(i=0; i<N; i++) {
        if(i == 0)
            fprintf(out, "[  ");
        else
            fprintf(out, "\n [  ");
        for(j=0; j<N; j++)
            fprintf(out, "%-4d", MAIN_MATRIX[i][j]);

        fprintf(out, "]");
    }
    fprintf(out, "]\n");
    fprintf(out, "Global sum is %d\n", global_sum);
    fclose(out);
}

/* Copy contents of a 5x5 submatrix to another 5x5 submatrix */
void init_submatrix(int** source, int*** dest) {
    int i, j;

    int** ret = (int**)malloc(SUBMATRIX_LIM * sizeof(int*));

    for(i=0; i<SUBMATRIX_LIM; i++) {
        ret[i] = (int *) calloc(SUBMATRIX_LIM, sizeof(int));

        for(j=0; j<SUBMATRIX_LIM; j++) {
            ret[i][j] = source[i][j];
        }
    }

    *(dest) = ret;
}