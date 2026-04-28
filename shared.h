#ifndef SHARED_H
#define SHARED_H

#include <sys/types.h>

#define NUM_RESOURCES 10
#define INSTANCES_PER_RESOURCE 5
#define MAX_PCB 20
#define PERMS 0700
#define BILLION 1000000000

typedef struct msgbuffer {
    long mtype;
    int intData;
} msgbuffer;

struct PCB {
    int occupied;
    pid_t pid;
    int start_sec;
    int start_nanosec;
    int end_sec;
    int end_nano;
    int event_wait_sec;
    int event_wait_nano;
    int blocked;
    int resources_allocated[NUM_RESOURCES];
    int requested_resource; // -1 if none, otherwise 0 to NUM_RESOURCES-1
    int messages_sent;
};

#endif
