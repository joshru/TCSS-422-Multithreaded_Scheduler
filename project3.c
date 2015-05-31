/**
 * Multi-Threaded fun!
 *
 * @author Josh Rueschenberg
 * @author Brandon Bell
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
 
#define SIZE 20
#define THREADS 16
#define NUM_CPU 8
#define NUM_IO 4
#define NUM_SUB 4
#define JOBS 20
#define WAIT 0
#define CPU 1
#define IO 2
 
// instantiate a bunch of locks for our various threads 
pthread_mutex_t ready_mutex =       PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ready_front_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cpu_mutex =         PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t io_mutex =          PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t job_mutex =         PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t complete_mutex =    PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t run_lock =          PTHREAD_MUTEX_INITIALIZER;
 
struct job {
    int id;           // unique job identifier
    int phases;       // number of phases
    int currentPhase; // current phase of the job
    int phaseType;    // 1 = CPU phase, 2 = I/O phase
    int duration;     // how long the job should run for
    int completed;    // 1 = completed, 0 = not yet completed
    int createdBy;    // which thread created the job
};
typedef struct job Job;
 
//implementing queues as arrays with front and end pointers for simplicity
Job *runQueue[JOBS], *IOqueue[JOBS], *finishedQueue[JOBS];
 
//ints to be used as pointers for our "queues" 
int runFront = 0;
int runEnd = 0;
int ioFront = 0;
int ioEnd = 0;
int finFront = 0;
int finEnd = 0;
int freecount = 0; //counting freed jobs
 
int ioQueueCount = 0;
int completeQueueCount = 0;
int readyCount = 0;         // use  this to measure how many jobs are on the ready queue.  
int cpuCount = 0;           // used this to measure when the threads run on cpu. When a thread runs on cpu it will increment this counter.
int ioCount = 0;            // used this to measure when threads run I/O, will increment when threads execute I/O
int jobsCompleted = 0;      // used this to monitor how many jobs have completed
int jobsCreated = 0;        // used to monitor how many jobs have been created
int jobID = 1;              // used to easily identify jobs will be a value 1 - JOBS

 
void runJob(Job *j) {
 
    sleep(j->duration);
 
 
    printf("Job %d ran for %ds. Current Phase: %d, Remaining Phases: %d\n\n", j->id, j->duration, j->currentPhase, (j->phases - j->currentPhase));
    j->currentPhase++;
   
    //flip the phase
    if (j->phaseType == 1)  j->phaseType = 2; //switch to io
    else                    j->phaseType = 1; //switch to cpu
 
    if (j->currentPhase <= j->phases) {
 
        //add job to appropriate queue
        if (j->phaseType == 1) {
                //add to ready queue
                pthread_mutex_lock(&ready_mutex);
            runQueue[runEnd++] = j;
            printf("Job %d placed in run queue at position %d\n", j->id, runEnd-1);
            readyCount++;
                pthread_mutex_unlock(&ready_mutex);
        } else if (j->phaseType == 2) {
                //add to io queue
                pthread_mutex_lock(&io_mutex);
 
            IOqueue[ioEnd++] = j;
            printf("Job %d placed in IO queue at position %d\n", j->id, ioEnd-1);
            ioQueueCount++;
 
                pthread_mutex_unlock(&io_mutex);
        }
 
    } else {
        //add to complete queue
        pthread_mutex_lock(&complete_mutex);
 
            finishedQueue[finEnd++] = j;
            printf("Job %d added to complete queue\n", j->id);
 
            completeQueueCount++;
          //  jobsCompleted++;
 
        pthread_mutex_unlock(&complete_mutex);
    }
}

//threads in this function will run jobs from the run queue
void* manageCpuPhase(void * thread) {
    int t = (int)(long) thread;
    printf("Thread %d entered manage CPU!\n", t);
   
    int result; //success variable. if a job is ready will be set to 1, 0 otherwise
 
    while (jobsCompleted < JOBS) {
        Job *j;
       
        pthread_mutex_lock(&ready_mutex);
        if (readyCount > 0) {
            j = runQueue[runFront++];
            result = 1;
            readyCount--;
             printf("Thread %d grabbed job %d from position %d, total completed: %d, num ready: %d\n",
                             t,            j->id,           runFront-1,          jobsCompleted,  readyCount+1);
        } else {
            result = 0;
        }
 
        pthread_mutex_unlock(&ready_mutex);
        if (result == 1) {
            printf("About to run job %d\n", j->id);
            runJob(j);
        }
    }
    pthread_exit(NULL);
}
 
//threads in this function will run jobs from the I/O queue
void* manageIO(void * thread) {
    int t = (int)(long) thread;
    printf("Thread %d entered manage I/O!\n", t);
   
    while (jobsCompleted < JOBS) {
        Job *j;
 
        int result;
        pthread_mutex_lock(&io_mutex);
        if (ioQueueCount > 0) {
             j = IOqueue[ioFront++];
             ioQueueCount--;
             result = 1;
             printf("Thread %d grabbed job id: %d from position %d, total completed: %d, num ready: %d\n",
                             t,                 j->id,        runFront-1,       jobsCompleted,    ioQueueCount+1);
 
        } else {
             result = 0;
        }
        pthread_mutex_unlock(&io_mutex);
 
        if (result == 1) {
             printf("About to run IO for ID: %d\n", j->id);
             runJob(j);
        }
    }
    pthread_exit(NULL);
}
 
//creates a job and adds it to the run queue
void createJob(int long t) {
    Job *j;
    int rPhases = rand() % 6 + 3; 
    int dur = rand() % 4 + 1;

    //lock it up, time to create and queue up a job
    pthread_mutex_lock(&ready_mutex);
    
    j = malloc(sizeof(Job)); // ignore error checking for malloc failure 
    memset(j, 0, sizeof(Job));
    //assign field values for newly allocated Job
    j->id = jobID++;
    j->phases = rPhases;
    j->currentPhase = 1;
    j->phaseType = 1;
    j->duration = dur;
    j->completed = 0;
    j->createdBy = t;
     
    jobsCreated++;
    runQueue[runEnd++] = j;
    readyCount++; //counter for run queue

    printf("Job %d created by thread %d, numPhases: %d, PhaseType: %d, duration: %d, position: %d\n",
    runQueue[runEnd-1]->id, j->createdBy, runQueue[runEnd-1]->phases, runQueue[runEnd-1]->phaseType, runQueue[runEnd-1]->duration, runEnd - 1);
    
    pthread_mutex_unlock(&ready_mutex);
        int naptime = 3;
        printf("Job created, taking a nap for %d\n", naptime);
        sleep(naptime);
}
 

//frees a completed job from the finished queue as long as the currently running thread matches the job's creator thread 
void freeCompleteJob(long int t) {
    pthread_mutex_lock(&complete_mutex);
    if (completeQueueCount > 0) {
       
        if (t == finishedQueue[finFront]->createdBy) {
            freecount++;
 
            printf("Thread %d freeing job %d (created by thread %d)\n", t, finishedQueue[finFront]->id, finishedQueue[finFront]->createdBy);
            free(finishedQueue[finFront++]);
            jobsCompleted++;
            completeQueueCount--;
 
        }
    }
     
    pthread_mutex_unlock(&complete_mutex);
 
       
}
 
 
//threads in this function create jobs at some frequency and submit them to the ready queue
void* manageJobSubmission(void * thread) {
    int delay = 10;
    int i;
    int t = (int)(long) thread;
    printf("Thread %d entered manage Jobs!\n", t);
   
    while (jobsCompleted < JOBS) {
        //if we need more jobs, make more jobs
        if (jobsCreated < JOBS) {
            createJob(t);
        }
        //check for completed jobs and free them
        freeCompleteJob(t);
    }
    pthread_exit(NULL);
}
 
int main(void) {
 
    srand(time(NULL)); //seed random number
    pthread_t CPUthreads[16];
    int i;

    printf("Now creating and running %d job(s)!\n\n", JOBS);
 
    // create our threads
    pthread_create(&CPUthreads[0],  NULL, manageCpuPhase,      (void *)(long) 1);
    pthread_create(&CPUthreads[1],  NULL, manageCpuPhase,      (void *)(long) 2);
    pthread_create(&CPUthreads[2],  NULL, manageCpuPhase,      (void *)(long) 3);
    pthread_create(&CPUthreads[3],  NULL, manageCpuPhase,      (void *)(long) 4);
    pthread_create(&CPUthreads[4],  NULL, manageCpuPhase,      (void *)(long) 5);
    pthread_create(&CPUthreads[5],  NULL, manageCpuPhase,      (void *)(long) 6);
    pthread_create(&CPUthreads[6],  NULL, manageCpuPhase,      (void *)(long) 7);
    pthread_create(&CPUthreads[7],  NULL, manageCpuPhase,      (void *)(long) 8);
    pthread_create(&CPUthreads[8],  NULL, manageIO,            (void *)(long) 9);
    pthread_create(&CPUthreads[9],  NULL, manageIO,            (void *)(long) 10);
    pthread_create(&CPUthreads[10], NULL, manageIO,            (void *)(long) 11);
    pthread_create(&CPUthreads[11], NULL, manageIO,            (void *)(long) 12);
    pthread_create(&CPUthreads[12], NULL, manageJobSubmission, (void *)(long) 13);
    pthread_create(&CPUthreads[13], NULL, manageJobSubmission, (void *)(long) 14);
    pthread_create(&CPUthreads[14], NULL, manageJobSubmission, (void *)(long) 15);
    pthread_create(&CPUthreads[15], NULL, manageJobSubmission, (void *)(long) 16);
 
    for (i = 0; i < THREADS; i++) {
        pthread_join(CPUthreads[i], NULL);
    }
 
    printf("Completed a total of %d jobs, Number of jobs left to free: %d, Number of Jobs freed: %d\n", jobsCompleted, completeQueueCount, freecount);
    pthread_mutex_destroy(&cpu_mutex);
    pthread_mutex_destroy(&io_mutex);
    pthread_mutex_destroy(&job_mutex);
    pthread_mutex_destroy(&ready_mutex);
    pthread_mutex_destroy(&complete_mutex);
    pthread_mutex_destroy(&run_lock);
    pthread_mutex_destroy(&ready_front_mutex);
    pthread_exit(NULL);
 
    return 0;
}