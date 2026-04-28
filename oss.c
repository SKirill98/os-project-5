#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/msg.h>
#include "shared.h"

// Global variables for signal cleanup
const int BUFF_SIZE = sizeof(int) * 2;
int shm_id;
int *clockptr;
int msqid;

void cleanup(int sig) {
    if (sig != 0) {
        printf("\nOSS: Caught signal %d. Cleaning up and terminating...\n", sig);
        // Ignore SIGTERM so we don't kill ourselves when we signal the process group
        signal(SIGTERM, SIG_IGN);
        kill(0, SIGTERM);
    }

    if (clockptr != NULL)
        shmdt(clockptr);
    shmctl(shm_id, IPC_RMID, NULL);
    msgctl(msqid, IPC_RMID, NULL);

    exit(sig == 0 ? 0 : 1);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    int opt;
    int n = -1, s = -1;
    double t = -1, i = -1;
    FILE *f = stdout;

    while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [-h] [-n proc] [-s simul] [-t timelimit] [-i interval] [-f logfile]\n", argv[0]);
                return 0;
            case 'n': n = atoi(optarg); break;
            case 's': s = atoi(optarg); break;
            case 't': t = atof(optarg); break;
            case 'i': i = atof(optarg); break;
            case 'f': f = fopen(optarg, "w"); break;
        }
    }

    if (n <= 0 || s <= 0 || t <= 0 || i < 0) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

    signal(SIGINT, cleanup);
    signal(SIGALRM, cleanup);
    alarm(60);

    key_t shm_key = ftok("oss.c", 'R');
    shm_id = shmget(shm_key, BUFF_SIZE, PERMS | IPC_CREAT);
    clockptr = (int *)shmat(shm_id, NULL, 0);
    int *sec = &clockptr[0];
    int *nano = &clockptr[1];
    *sec = 0; *nano = 0;

    key_t msg_key = ftok("msgq.txt", 1);
    if (msg_key == -1) {
        FILE *tmp = fopen("msgq.txt", "w");
        fclose(tmp);
        msg_key = ftok("msgq.txt", 1);
    }
    msqid = msgget(msg_key, PERMS | IPC_CREAT);

    struct PCB table[MAX_PCB];
    memset(table, 0, sizeof(table));
    for(int j=0; j<MAX_PCB; j++) table[j].requested_resource = -1;

    int resource_available[NUM_RESOURCES];
    for (int j = 0; j < NUM_RESOURCES; j++) resource_available[j] = INSTANCES_PER_RESOURCE;

    int running = 0, total_launched = 0, total_requests = 0, granted_immediately = 0;
    int last_launch_sec = 0, last_launch_nano = 0;
    int last_print_sec = -1, last_print_nano = -1;
    int current = -1;
    msgbuffer msg;

    while (total_launched < n || running > 0) {
        // Increment clock
        *nano += 10000000;
        if (*nano >= BILLION) { (*sec)++; *nano -= BILLION; }

        // Check for unblocking
        for (int j = 0; j < MAX_PCB; j++) {
            if (table[j].occupied && table[j].blocked) {
                int res = table[j].requested_resource;
                if (res != -1 && resource_available[res] > 0) {
                    resource_available[res]--;
                    table[j].resources_allocated[res]++;
                    table[j].blocked = 0;
                    table[j].requested_resource = -1;
                    fprintf(f, "Master granting P%d blocked request for R%d at time %d:%d\n", j, res, *sec, *nano);
                    msg.mtype = table[j].pid;
                    msg.intData = 1; // Granted
                    msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
                }
            }
        }

        // Launch new worker
        int diff_sec = *sec - last_launch_sec;
        int diff_nano = *nano - last_launch_nano;
        if (diff_nano < 0) { diff_sec--; diff_nano += BILLION; }
        if (running < s && total_launched < n && running < 18 && (total_launched == 0 || (diff_sec > (int)i || (diff_sec == (int)i && diff_nano >= (i - (int)i) * BILLION)))) {
            int index = -1;
            for (int j = 0; j < MAX_PCB; j++) if (!table[j].occupied) { index = j; break; }
            if (index != -1) {
                pid_t child = fork();
                if (child == 0) {
                    char tStr[20]; sprintf(tStr, "%f", t);
                    execl("./worker", "worker", tStr, NULL);
                    exit(1);
                }
                table[index].occupied = 1; table[index].pid = child;
                table[index].start_sec = *sec; table[index].start_nanosec = *nano;
                running++; total_launched++;
                last_launch_sec = *sec; last_launch_nano = *nano;
                fprintf(f, "Master launching P%d at time %d:%d\n", index, *sec, *nano);
            }
        }

        // Pick next unblocked worker
        int found = -1;
        for (int k = 0; k < MAX_PCB; k++) {
            current = (current + 1) % MAX_PCB;
            if (table[current].occupied && !table[current].blocked) { found = current; break; }
        }

        if (found != -1) {
            msg.mtype = table[found].pid;
            msg.intData = 1; // Go ahead
            msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);

            msgrcv(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 1, 0);
            if (msg.intData == 0) { // Terminating
                fprintf(f, "Master has acknowledged Process P%d terminating at time %d:%d\n", found, *sec, *nano);
                for (int j = 0; j < NUM_RESOURCES; j++) {
                    resource_available[j] += table[found].resources_allocated[j];
                    table[found].resources_allocated[j] = 0;
                }
                waitpid(table[found].pid, NULL, 0);
                table[found].occupied = 0;
                running--;
            } else if (msg.intData > 0) { // Request
                int res = msg.intData - 1;
                total_requests++;
                fprintf(f, "Master has detected Process P%d requesting R%d at time %d:%d\n", found, res, *sec, *nano);
                if (resource_available[res] > 0) {
                    resource_available[res]--;
                    table[found].resources_allocated[res]++;
                    granted_immediately++;
                    fprintf(f, "Master granting P%d request R%d at time %d:%d\n", found, res, *sec, *nano);
                    msg.mtype = table[found].pid;
                    msg.intData = 1; // Granted
                    msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
                } else {
                    table[found].blocked = 1;
                    table[found].requested_resource = res;
                    fprintf(f, "Master blocking P%d for R%d at time %d:%d\n", found, res, *sec, *nano);
                }
            } else if (msg.intData < 0) { // Release
                int res = abs(msg.intData) - 1;
                fprintf(f, "Master has acknowledged Process P%d releasing R%d at time %d:%d\n", found, res, *sec, *nano);
                resource_available[res]++;
                table[found].resources_allocated[res]--;
                msg.mtype = table[found].pid;
                msg.intData = 1; // Acknowledged
                msgsnd(msqid, &msg, sizeof(msgbuffer) - sizeof(long), 0);
            }
        }

        // Print tables every 0.5s
        if (*sec > last_print_sec || (*sec == last_print_sec && *nano >= last_print_nano + 500000000)) {
            last_print_sec = *sec; last_print_nano = *nano;
            fprintf(f, "\nOSS PID:%d SysClockS:%d SysClockNano:%d\n", getpid(), *sec, *nano);
            fprintf(f, "Resources available:\n");
            for(int j=0; j<NUM_RESOURCES; j++) fprintf(f, "R%d:%d ", j, resource_available[j]);
            fprintf(f, "\nResources allocated:\n     ");
            for(int j=0; j<NUM_RESOURCES; j++) fprintf(f, "R%2d ", j);
            fprintf(f, "\n");
            for(int i=0; i<MAX_PCB; i++) {
                if(table[i].occupied) {
                    fprintf(f, "P%2d: ", i);
                    for(int j=0; j<NUM_RESOURCES; j++) fprintf(f, "%3d ", table[i].resources_allocated[j]);
                    fprintf(f, "\n");
                }
            }
        }
    }

    fprintf(f, "\nFinal Report:\nTotal requests: %d\nGranted immediately: %d (%.2f%%)\n", total_requests, granted_immediately, total_requests > 0 ? (float)granted_immediately/total_requests*100 : 0);
    cleanup(0);
    return 0;
}