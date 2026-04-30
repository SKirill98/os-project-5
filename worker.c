#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <time.h>
#include "shared.h"

int shm_key;
int shm_id;

int main(int argc, char *argv[]) {
    // Check whether the right number of arguments were passed.
    if (argc != 3) {
        fprintf(stderr, "Usage: worker sec nano\n");
        exit(1);
    }

    int term_sec = atoi(argv[1]);
    int term_nano = atoi(argv[2]);
    srand(time(NULL) ^ (getpid() << 16));

    // Shared memory
    shm_key = ftok("oss.c", 'R');
    if (shm_key <= 0 ) {
        fprintf(stderr,"Child:... Error in ftok\n");
        exit(1);
    }
    
    // Create shared memory segment
    shm_id = shmget(shm_key, sizeof(int)*2, PERMS);
    if (shm_id <= 0 ) {
        fprintf(stderr,"child:... Error in shmget\n");
        exit(1);
    }

    // Attach to shared memory
    int *clockptr = (int *)shmat(shm_id, NULL, 0);
    int *sec = &clockptr[0];
    int *nano = &clockptr[1];

    // Get a key for our message queue
    key_t msg_key;
    if ((msg_key = ftok("msgq.txt", 1)) == -1) {
        perror("ftok");
        exit(1);
    }
    // Create message queue
    int msqid = 0;    
    if ((msqid = msgget(msg_key, PERMS)) == -1) {
        perror("msgget in child");
        exit(1);
    }

    printf("Worker %d has access to the queue \n", getpid());

    int resources_held[NUM_RESOURCES] = {0};

    printf("WORKER PID:%d PPID:%d\n", getpid(), getppid());
    printf("SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d\n", *sec, *nano, term_sec, term_nano);
    printf("--Just Starting\n");

    msgbuffer msg;

    while (1) {
        // Wait for permission from oss
        if (msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0) == -1) break;

        // Check if time to terminate
        if (*sec > term_sec || (*sec == term_sec && *nano >= term_nano)) {
            printf("WORKER PID:%d PPID:%d\n", getpid(), getppid());
            printf("SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d\n", *sec, *nano, term_sec, term_nano);
            printf("--Terminating\n");
            msg.mtype = 1;
            msg.intData = 0; // Terminate
            msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
            break;
        }

        // Decide: Request (80%) or Release (20%)
        int action = rand() % 100;
        if (action < 80) {
            // Request
            int res = rand() % NUM_RESOURCES;
            if (resources_held[res] < INSTANCES_PER_RESOURCE) { // There are available instances of these resources to request
                msg.mtype = 1;
                msg.intData = res + 1;
                msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
                
                // Wait for grant
                msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0);
                resources_held[res]++;
            } else {
                // Cannot request more of this resource
                int found = -1;
                for(int j=0; j<NUM_RESOURCES; j++) if(resources_held[j] < INSTANCES_PER_RESOURCE) { found = j; break; }
                if (found != -1) { // Can request a different resource
                    msg.mtype = 1;
                    msg.intData = found + 1;
                    msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);

                    // Wait for grant
                    msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0);
                    resources_held[found]++;
                } else {
                    // Holding all resources? Release one instead of requesting
                    printf("WORKER PID:%d PPID:%d\n", getpid(), getppid());
                    printf("SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d\n", *sec, *nano, term_sec, term_nano);
                    printf("--Holding max of all resources, releasing one instead of requesting\n");
                    
                    int res;
                    do { res = rand() % NUM_RESOURCES; } while (resources_held[res] == 0);
                    msg.mtype = 1;
                    msg.intData = -(res + 1);
                    msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
                
                    // Wait for ACK
                    msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0);
                    resources_held[res]--;
                }
            }
        } else {
            // Release
            int has_any = 0;
            for(int j=0; j<NUM_RESOURCES; j++) if(resources_held[j] > 0) has_any = 1;

            if (has_any) {
                int res;
                do { res = rand() % NUM_RESOURCES; } while (resources_held[res] == 0);
                msg.mtype = 1;
                msg.intData = -(res + 1);
                msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
                
                // Wait for ACK
                msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0);
                resources_held[res]--;
            } else {
                // Nothing to release, just request instead
                int res = rand() % NUM_RESOURCES;
                msg.mtype = 1;
                msg.intData = res + 1;
                msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
                
                // Wait for grant
                msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0);
                resources_held[res]++;
            }
        }
    }

    shmdt(clockptr);
    return 0;
}