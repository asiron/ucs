//
//  server.c
//  unix-communicator-server
//
//  Created by Maciej Żurad on 1/26/13.
//  Copyright (c) 2013 Maciej Żurad. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <time.h>

#include "protocol.h"
#include "server.h"

int main(int argc, const char * argv[])
{
    init_server();

    clean();
    return 0;
}

void init_server(){
    init_sems();
    init_repo();
    
    MSG_RECEIVER = msgget(IPC_PRIVATE, 0666);
    
    lock_repo();
    register_new_server(MSG_RECEIVER);
    unlock_repo();

    
}

void init_sems(){
    
    if( (SEM_SERVER_IDS = semget(SEM_SERVER_IDS_KEY, 1, 0666 | IPC_CREAT | IPC_EXCL) ) == -1 ) {
        perror("Semaphore SEM_SERVER_IDS already exists, not creating new one");
        SEM_SERVER_IDS = semget(SEM_SERVER_IDS_KEY, 1, 0666);
    } else {
        semctl(SEM_SERVER_IDS, 0, SETVAL, 1);
    }
    
    if( (SEM_ROOM_SERVER = semget(SEM_ROOM_SERVER_KEY, 1, 0666 | IPC_CREAT | IPC_EXCL) ) == -1 ) {
        perror("Semaphore SEM_ROOM_SERVER_KEY already exists, not creating new one");
        SEM_ROOM_SERVER = semget(SEM_ROOM_SERVER_KEY, 1, 0666);
    } else {
        semctl(SEM_ROOM_SERVER, 0, SETVAL, 1);
    }
    
    if( (SEM_USER_SERVER = semget(SEM_USER_SERVER_KEY, 1, 0666 | IPC_CREAT | IPC_EXCL) ) == -1 ) {
        perror("Semaphore SEM_USER_SERVER already exists, not creating new one");
        SEM_USER_SERVER = semget(SEM_USER_SERVER_KEY, 1, 0666);
    } else {
        semctl(SEM_USER_SERVER, 0, SETVAL, 1);
    }

    if( (SEM_LOGFILE = semget(SEM_LOGFILE_KEY, 1, 0666 | IPC_CREAT | IPC_EXCL) ) == -1 ) {
        perror("Semaphore SEM_LOGFILE already exists, not creating new one");
        SEM_LOGFILE = semget(SEM_LOGFILE_KEY, 1, 0666);
    } else {
        semctl(SEM_LOGFILE, 0, SETVAL, 1);
    }

}

void lock_repo(){
    sem_down(SEM_SERVER_IDS);
    sem_down(SEM_ROOM_SERVER);
    sem_down(SEM_USER_SERVER);
}

void unlock_repo(){
    sem_up(SEM_USER_SERVER);
    sem_up(SEM_ROOM_SERVER);
    sem_up(SEM_SERVER_IDS);
}

void lock_log(){
    sem_down(SEM_LOGFILE);
}

void unlock_log(){
    sem_up(SEM_LOGFILE);
}

void sem_up(int sem){
    struct sembuf s;
    s.sem_num = 0;
    s.sem_op = 1;
    s.sem_flg = 0;
    semop(sem, &s, 1);
}

void sem_down(int sem){
    struct sembuf s;
    s.sem_num = 0;
    s.sem_op = -1;
    s.sem_flg = 0;
    semop(sem, &s, 1);
}


void init_repo() {

lock_repo();
    
    if( ( SHM_SERVER_IDS = shmget(SHM_SERVER_IDS_KEY, REPO_SIZE*sizeof(int), 0666 | IPC_CREAT | IPC_EXCL) ) == -1) {
        perror("Shared memory SHM_SERVER_IDS already exists, not creating new one");
        SHM_SERVER_IDS = shmget(SHM_SERVER_IDS_KEY, REPO_SIZE*sizeof(int), 0666);
        SHM_SERVER_IDS_ADRESS = (int*)shmat(SHM_SERVER_IDS, NULL, 0);
    } else {
        SHM_SERVER_IDS_ADRESS = (int*)shmat(SHM_SERVER_IDS, NULL, 0);
        int i;
        for(i=0; i<REPO_SIZE; ++i){
            SHM_SERVER_IDS_ADRESS[i] = -1;
        }
    }
    
    if( ( SHM_ROOM_SERVER = shmget(SHM_ROOM_SERVER_KEY, REPO_SIZE*sizeof(ROOM_SERVER), 0666 | IPC_CREAT | IPC_EXCL) ) == -1) {
        perror("Shared memory SHM_ROOM_SERVER already exists, not creating new one");
        SHM_ROOM_SERVER = shmget(SHM_ROOM_SERVER_KEY, REPO_SIZE*sizeof(ROOM_SERVER), 0666);
        SHM_ROOM_SERVER_ADRESS = (ROOM_SERVER*)shmat(SHM_ROOM_SERVER, NULL, 0);
    } else {
        SHM_ROOM_SERVER_ADRESS = (ROOM_SERVER*)shmat(SHM_ROOM_SERVER, NULL, 0);
        int i;
        for(i=0; i<REPO_SIZE; ++i){
            strcpy(SHM_ROOM_SERVER_ADRESS[i].room_name, "");
            SHM_ROOM_SERVER_ADRESS[i].server_id = -1;
        }
    }
    
    if( ( SHM_USER_SERVER = shmget(SHM_USER_SERVER_KEY, REPO_SIZE*sizeof(USER_SERVER), 0666 | IPC_CREAT | IPC_EXCL) ) == -1) {
        perror("Shared memory SHM_USER_SERVER already exists, not creating new one");
        SHM_USER_SERVER = shmget(SHM_USER_SERVER_KEY, REPO_SIZE*sizeof(USER_SERVER), 0666);
        SHM_USER_SERVER_ADRESS = (USER_SERVER*)shmat(SHM_USER_SERVER, NULL, 0);
    } else {
        SHM_USER_SERVER_ADRESS = (USER_SERVER*)shmat(SHM_USER_SERVER, NULL, 0);
        int i;
        for(i=0; i<REPO_SIZE; ++i){
            strcpy(SHM_USER_SERVER_ADRESS[i].user_name, "");
            SHM_USER_SERVER_ADRESS[i].server_id = -1;
        }
    }
    
unlock_repo();

}


void register_new_server(){
    int i;
    for (i=0; i<REPO_SIZE; ++i) {
        if( SHM_SERVER_IDS_ADRESS[i] == -1){
            SHM_SERVER_IDS_ADRESS[i] = MSG_RECEIVER;
            log_data("ALIVE");
            break;
        }
    }
}

void clean(){
    int i;
    for (i=0; i<REPO_SIZE; ++i) {
        if (SHM_SERVER_IDS_ADRESS[i] == MSG_RECEIVER) {
            SHM_SERVER_IDS_ADRESS[i] = -1;
        }
    }
    release_resources();
    log_data("DEAD");
}

void release_resources(){
    msgctl(MSG_RECEIVER, IPC_RMID, 0);
}

void log_data(const char* text, ...){
lock_log();
    FILE* log = NULL;
    if( !(log = fopen(LOG_FILENAME, "at") )){
        log = fopen(LOG_FILENAME, "wt");
    }
    
    char front_text[40];
    char back_text[100];
    char formatted_time[24];
    
    time_t current_time;
    time(&current_time);
    
    strncpy(formatted_time, ctime(&current_time), 24);
    formatted_time[24] = '\0';
    printf("%s \n", formatted_time);
    sprintf(front_text, "%d\t%s\t", MSG_RECEIVER, formatted_time);
    
    va_list args;
    va_start(args, text);
    vsprintf(back_text, text, args);
    va_end(args);
    fprintf(log, "%s%s\n", front_text, back_text);
    fclose(log);
unlock_log();
}

