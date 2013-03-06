//
//  server.c
//  unix-communicator-server
//
//  Created by Maciej Żurad on 1/26/13.
//  Copyright (c) 2013 Maciej Żurad. All rights reserved.
//

#include "protocol.h"
#include "server.h"

int main(int argc, const char * argv[])
{
    init_server();
    void* received = malloc(2048);
    while(1){
        receive_msg(received, _size(MSG_LOGIN), LOGIN, &handle_login);
//        receive_msg(received, _size(MSG_LOGIN), LOGOUT, &handle_logout);
//        receive_msg(received, _size(MSG_REQUEST), REQUEST, &handle_request);
//        receive_msg(received, _size(MSG_CHAT_MESSAGE), MESSAGE, &handle_message);
//        receive_msg(received, _size(MSG_ROOM), ROOM, &handle_room_action);
//        receive_msg(received, _size(MSG_SERVER2SERVER), SERVER2SERVER, &handle_server_heartbeat);
        usleep(100);
    }
    free(received);
    clean();
    return 0;
}
void handle_login(void* received, int msg_type){
    MSG_LOGIN temp = *(MSG_LOGIN*)(received);
    MSG_RESPONSE response;
    response.type = RESPONSE;
    switch (register_new_user(temp) ) {
        case SUCCESS:
            add_to_local_repo(temp.ipc_num, temp.username, "default");
            response.response_type = LOGIN_SUCCESS;
            strcpy(response.content, "LOGIN SUCCESS - 200");
            msgsnd(temp.ipc_num, &response, _size(MSG_RESPONSE), 0);
            break;
        case EXISTS:
            response.response_type = LOGIN_FAILED;
            strcpy(response.content, "LOGIN FAILED - USER ALREADY EXISTS");
            msgsnd(temp.ipc_num, &response, _size(MSG_RESPONSE), 0);
            break;
        case FULL:
            response.response_type = LOGIN_FAILED;
            strcpy(response.content, "LOGIN FAILED - SERVER FULL");
            msgsnd(temp.ipc_num, &response, _size(MSG_RESPONSE), 0);
            break;
    }
}
void handle_logout(void* received, int msg_type){
    MSG_LOGIN temp = *(MSG_LOGIN*)(received);
    MSG_RESPONSE response;
    response.type = RESPONSE;
    switch (unregister_user(temp)) {
        case SUCCESS:
            remove_user_from_local_repo(temp.ipc_num);
            response.response_type = LOGOUT_SUCCESS;
            strcpy(response.content, "LOGOUT SUCCESS - 200");
            msgsnd(temp.ipc_num, &response, _size(MSG_RESPONSE), 0);
            break;
        case FAIL:
            response.response_type = LOGOUT_FAILED;
            strcpy(response.content, "LOGOUT FAILED");
            msgsnd(temp.ipc_num, &response, _size(MSG_RESPONSE), 0);
            break;
    }
}
void handle_request(void* received, int msg_type){

}
void handle_message(void* received, int msg_type){

}
void handle_room_action(void* received, int msg_type){

}
void handle_server_heartbeat(void* received, int msg_type){

}

int unregister_user(MSG_LOGIN user){
lock_repo();
    int return_flag = FAIL;
    int i;
    for(i=0; i<REPO_SIZE; ++i){
        if( SHM_USER_SERVER_ADRESS[i].server_id == user.ipc_num){
            SHM_USER_SERVER_ADRESS[i].server_id = -1;
            strcpy(SHM_USER_SERVER_ADRESS[i].user_name, "");
            return_flag =  SUCCESS;
            break;
        }
    }
unlock_repo();
    return return_flag;
}

int register_new_user(MSG_LOGIN user){
lock_repo();
    int return_flag = FULL;
    int i;
    for(i=0; i<REPO_SIZE; ++i){
        if( SHM_USER_SERVER_ADRESS[i].server_id == -1){
            strcpy(SHM_USER_SERVER_ADRESS[i].user_name, user.username);
            SHM_USER_SERVER_ADRESS[i].server_id = user.ipc_num;
            log_data("USER FROM %d JOINED AS %s", user.ipc_num, user.username);
            return_flag = SUCCESS;
            break;
        } else if ( !strcmp(SHM_USER_SERVER_ADRESS[i].user_name, user.username) ) {
            log_data("USER FROM %d WAS REJECT WHEN TRYING TO JOIN AS %s, NAME ALREADY TAKEN",user.ipc_num, user.username);
            return_flag = EXISTS;
            break;
        }
    }
unlock_repo();
    if(return_flag == FULL)
        log_data("USER FROM %d WAS REJECT WHEN TRYING TO JOIN AS %s, SERVER FULL",user.ipc_num, user.username);
    return return_flag;
}

void init_local_repo(){
    int i;
    for(i=0; i<MAX_USERS_NUMBER; ++i){
        LOCAL_REPO[i].client_id = -1;
        strcpy(LOCAL_REPO[i].room_name, "default");
        strcpy(LOCAL_REPO[i].user_name, "");
    }
}

void add_to_local_repo(int client_id, const char* username, const char* roomname){
    int i;
    for(i=0; i<MAX_USERS_NUMBER; ++i){
        if(LOCAL_REPO[i].client_id == -1){
            LOCAL_REPO[i].client_id = client_id;
            strcpy(LOCAL_REPO[i].user_name, username);
            strcpy(LOCAL_REPO[i].room_name, roomname);
            return;
        }
    }
    printf("Somehow there is no space to add user even tho we already checked that... EXITING\n");
    clean();
    exit(EXIT_FAILURE);
}

void remove_user_from_local_repo(int client_id){
    int i;
    for(i=0; i<MAX_USERS_NUMBER; ++i){
        if (LOCAL_REPO[i].client_id == client_id) {
            LOCAL_REPO[i].client_id = -1;
            strcpy(LOCAL_REPO[i].user_name, "");
            strcpy(LOCAL_REPO[i].room_name, "default");
            return;
        }
    }
}

void receive_msg(void* received, size_t msg_size, int  msg_type, fnc_handler handler) {
    if ( msgrcv(MSG_RECEIVER, received, msg_size, msg_type, IPC_NOWAIT) != -1 ) {
        handler(received, msg_type);
    }
}

void init_server(){
    
    signal(SIGINT, clean);
    
    MSG_RECEIVER = msgget(IPC_PRIVATE, 0666);

    init_sems();
    init_repo();
    init_local_repo();
    

    register_new_server(MSG_RECEIVER);

    
    printf("SERVER READY, LISTENING ON QUEUE %d\n", MSG_RECEIVER);
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
        log_data("CREATING REPOSITORY");
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

lock_repo();
    int i;
    for (i=0; i<REPO_SIZE; ++i) {
        if( SHM_SERVER_IDS_ADRESS[i] == -1){
            SHM_SERVER_IDS_ADRESS[i] = MSG_RECEIVER;
            log_data("ALIVE");
            break;
        }
    }
unlock_repo();
}

void clean(){
    int i;
    int flag = FALSE;
lock_repo();
    for (i=0; i<REPO_SIZE; ++i) {
        if (SHM_SERVER_IDS_ADRESS[i] == MSG_RECEIVER) {
            SHM_SERVER_IDS_ADRESS[i] = -1;
        } else if (SHM_SERVER_IDS_ADRESS[i] != -1) {
            flag = TRUE;
        }
    }
unlock_repo();
    
    log_data("DEAD");
    
    //we are last server so we clean whole repo
    if(!flag){
        log_data("CLEANING REPOSITORY");
        clean_repo();
    }

    release_resources();

    signal(SIGINT, SIG_DFL);
    exit(EXIT_SUCCESS);
}

void release_resources(){
    msgctl(MSG_RECEIVER, IPC_RMID, 0);
}

void clean_repo(){
lock_repo();
    shmdt(SHM_SERVER_IDS_ADRESS);
    shmdt(SHM_ROOM_SERVER_ADRESS);
    shmdt(SHM_SERVER_IDS_ADRESS);
    shmctl(SHM_ROOM_SERVER, IPC_RMID, 0);
    shmctl(SHM_SERVER_IDS, IPC_RMID, 0);
    shmctl(SHM_USER_SERVER, IPC_RMID, 0);
unlock_repo();
    semctl(SEM_LOGFILE, IPC_RMID, 0);
    semctl(SEM_ROOM_SERVER, IPC_RMID, 0);
    semctl(SEM_SERVER_IDS, IPC_RMID, 0);
    semctl(SEM_USER_SERVER, IPC_RMID, 0);
}

void log_data(const char* text, ...){
lock_log();
    FILE* log = NULL;
    if( !(log = fopen(LOG_FILENAME, "at") )){
        log = fopen(LOG_FILENAME, "wt");
    }
    
    char front_text[40];
    char back_text[100];
    char formatted_time[25];
    
    time_t current_time;
    time(&current_time);
    
    strncpy(formatted_time, ctime(&current_time), 24);
    formatted_time[24] = '\0';
    sprintf(front_text, "%d\t%s\t", MSG_RECEIVER, formatted_time);
    
    va_list args;
    va_start(args, text);
    vsprintf(back_text, text, args);
    va_end(args);
    fprintf(log, "%s%s\n", front_text, back_text);
    fclose(log);
unlock_log();
}



