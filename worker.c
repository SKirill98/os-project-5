#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <time.h>
#include "shared.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: worker t\n");
        exit(1);
    }

    double t_limit = atof(argv[1]);
    srand(time(NULL) ^ (getpid() << 16));

    key_t shm_key = ftok("oss.c", 'R');
    int shm_id = shmget(shm_key, sizeof(int) * 2, PERMS);
    int *clockptr = (int *)shmat(shm_id, NULL, 0);
    int *sec = &clockptr[0];
    int *nano = &clockptr[1];

    key_t msg_key = ftok("msgq.txt", 1);
    int msqid = msgget(msg_key, PERMS);

    int resources_held[NUM_RESOURCES] = {0};
    
    // Determine termination time
    int run_sec = (int)(((double)rand() / RAND_MAX) * t_limit);
    int run_nano = (int)(((double)rand() / RAND_MAX) * 1000000000);
    int term_sec = *sec + run_sec;
    int term_nano = *nano + run_nano;
    if (term_nano >= 1000000000) { term_sec++; term_nano -= 1000000000; }

    msgbuffer msg;

    while (1) {
        // Wait for permission from oss
        if (msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0) == -1) break;

        // Check if time to terminate
        if (*sec > term_sec || (*sec == term_sec && *nano >= term_nano)) {
            msg.mtype = 1;
            msg.intData = 0; // Terminate
            msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
            break;
        }

        // Decide: Request (70%) or Release (30%)
        int action = rand() % 100;
        if (action < 70) {
            // Request
            int res = rand() % NUM_RESOURCES;
            if (resources_held[res] < INSTANCES_PER_RESOURCE) {
                msg.mtype = 1;
                msg.intData = res + 1;
                msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
                
                // Wait for grant
                msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0);
                resources_held[res]++;
            } else {
                // Cannot request more of this resource, just tell oss we are still alive
                // Actually, oss expects a message back if it gave us permission.
                // But my oss loop waits for a response after sending permission.
                // So I MUST send something.
                // Let's just try to release something instead or just send a dummy release if possible.
                // Or just pick another resource.
                int found = -1;
                for(int j=0; j<NUM_RESOURCES; j++) if(resources_held[j] < INSTANCES_PER_RESOURCE) { found = j; break; }
                if (found != -1) {
                    msg.mtype = 1;
                    msg.intData = found + 1;
                    msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
                    msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0);
                    resources_held[found]++;
                } else {
                    // Holding all resources? Terminate.
                    msg.mtype = 1;
                    msg.intData = 0;
                    msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
                    break;
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
                msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0);
                resources_held[res]++;
            }
        }
    }

    shmdt(clockptr);
    return 0;
}