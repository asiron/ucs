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
    time_t send_hb_time = time(0);
    time_t check_hb_time = time(0);
    while(1){
        
        
        receive_msg(received, _size(MSG_LOGIN), LOGIN, &handle_login);
        receive_msg(received, _size(MSG_LOGIN), LOGOUT, &handle_logout);
        receive_msg(received, _size(MSG_REQUEST), REQUEST, &handle_request);
        receive_msg(received, _size(MSG_CHAT_MESSAGE), MESSAGE, &handle_message);
        receive_msg(received, _size(MSG_ROOM), ROOM, &handle_room_action);
        receive_msg(received, _size(MSG_SERVER2SERVER), SERVER2SERVER, &handle_server_heartbeat);
        
        if (time(0) - send_hb_time > 2) {
            send_hb_time = time(0);
            send_heartbeats_to_clients();
        }
        if (time(0) - check_hb_time > 2) {
            check_hb_time = time(0);
            check_heartbeat_table();
        }
        
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
            add_to_local_repo(temp.ipc_num, temp.username, "");
            response.response_type = LOGIN_SUCCESS;
            strcpy(response.content, "LOGIN SUCCESS - Connected to \'<no>\' channel");
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
    MSG_REQUEST temp = *(MSG_REQUEST*)(received);
    if (temp.request_type == PONG) {
        if ( time(0) - get_user_hbtime(temp.user_name) > 1){
            kick_user(temp.user_name, get_user_id(temp.user_name));
        }
    } else {
        MSG_USERS_LIST *list = get_list(temp.request_type);
        msgsnd(get_user_id(temp.user_name), list, _size(MSG_USERS_LIST), 0);
        free(list);
    }
}
void handle_message(void* received, int msg_type){
    MSG_CHAT_MESSAGE msg = *(MSG_CHAT_MESSAGE*)(received);
    
    MSG_RESPONSE rsp;
    rsp.type = RESPONSE;
    
    if (msg.msg_type == PUBLIC) {
        
        if (is_local_user(msg.sender)) {
            // if sender send msg to its parent server

            int servers_to_removed[REPO_SIZE];
            int servers_to_send[REPO_SIZE];
            
            int i;
            
            for (i=0; i<REPO_SIZE; ++i) {
                servers_to_send[i] = 0;
                servers_to_removed[i] = 0;
            }
        
            lock_repo();
                int j,k;
                int all_servers_exists = TRUE;
                for (i=0, j=0, k=0; i<REPO_SIZE; ++i) {
                    if ( !strcmp(SHM_ROOM_SERVER_ADRESS[i].room_name, msg.receiver) &&
                    SHM_ROOM_SERVER_ADRESS[i].server_id != MSG_RECEIVER &&
                    SHM_ROOM_SERVER_ADRESS[i].server_id != -1   ) {
                        if (!await_server_response(SHM_ROOM_SERVER_ADRESS[i].server_id)) {
                            all_servers_exists = FALSE;
                            servers_to_removed[j] = SHM_ROOM_SERVER_ADRESS[i].server_id;
                            ++j;
                        } else {
                            servers_to_send[k] = SHM_ROOM_SERVER_ADRESS[i].server_id;
                            ++k;
                        }
                    }
                }
            unlock_repo();
            
            if (all_servers_exists) {
                for (i=0; i<REPO_SIZE; ++i) {
                    if (servers_to_send[i]) {
                        msgsnd(servers_to_send[i], &msg, _size(MSG_CHAT_MESSAGE), 0);
                    }
                }
            } else {
                for (i=0; i<REPO_SIZE; ++i) {
                    if(servers_to_removed[i]) {
                        remove_server(servers_to_removed[i]);
                    }
                }
                rsp.response_type = MSG_NOT_SEND;
                strcpy(rsp.content, "NOT EVERY SERVER RESPONDED");
            }
            
            
            for (i=0; i<MAX_USERS_NUMBER; ++i) {
                if( !strcmp(LOCAL_REPO[i].room_name, msg.receiver) ) {
                    msgsnd(LOCAL_REPO[i].client_id, &msg, _size(MSG_CHAT_MESSAGE), 0);
                }
            }
        } else {
            // if we are the second server and we send msg to users
            
            int i;
            for (i=0; i<MAX_USERS_NUMBER; ++i) {
                if( !strcmp(LOCAL_REPO[i].room_name, msg.receiver) ) {
                    msgsnd(LOCAL_REPO[i].client_id, &msg, _size(MSG_CHAT_MESSAGE), 0);
                }
            }
            
            
        }
        
    } else if (msg.msg_type == PRIVATE) {
        
        int node_server_id = check_if_user_exists(msg.receiver);
        
        if(node_server_id == MSG_RECEIVER) {
            msgsnd(get_user_id(msg.receiver), &msg, _size(MSG_CHAT_MESSAGE), 0);
        } else if (node_server_id != FALSE) {
            if (await_server_response(node_server_id)) {
            
                msgsnd(node_server_id, &msg, _size(MSG_CHAT_MESSAGE), 0);
            } else {
                rsp.response_type = MSG_NOT_SEND;
                strcpy(rsp.content, "SERVER DID NOT RESPOND");
                remove_server(node_server_id);
            }
        } else if (node_server_id == FALSE) {
            rsp.response_type = MSG_NOT_SEND;
            strcpy(rsp.content, "USER DOESNT EXIST!");
        }
    }
    msgsnd(get_user_id(msg.sender), &rsp, _size(MSG_RESPONSE), 0);
}

void handle_room_action(void* received, int msg_type){
    MSG_ROOM temp = *(MSG_ROOM*)(received);
    MSG_RESPONSE response;
    response.type = RESPONSE;
    int client_id = get_user_id(temp.user_name);
    switch (temp.operation_type) {
        case ENTER_ROOM:
            if ( change_room(temp, client_id) == FULL ) {
                response.response_type = ENTERED_ROOM_FAILED;
                strcpy(response.content, "No space for new channel\n");
            } else {
                response.response_type = ENTERED_ROOM_SUCCESS;
                strcpy(response.content, "Channel successfuly entered!\n");
            }
            break;
        case LEAVE_ROOM:
            strcpy(temp.room_name, "");
            if ( change_room(temp, client_id) == FULL ) {
                response.response_type = LEAVE_ROOM_FAILED;
                strcpy(response.content, "No space for new channel\n");
            } else {
                response.response_type = LEAVE_ROOM_SUCCESS;
                strcpy(response.content, "Channel successfuly left!\n");
            }
            break;
        case CHANGE_ROOM:
            if ( change_room(temp, client_id) == FULL ) {
                response.response_type = CHANGE_ROOM_FAILED;
                strcpy(response.content, "No space for new channel\n");
            } else {
                response.response_type = CHANGE_ROOM_SUCCESS;
                strcpy(response.content, "Channel successfuly changed!\n");
            }
            break;
    }
    msgsnd(client_id, &response, _size(MSG_RESPONSE), 0);
}
void handle_server_heartbeat(void* received, int msg_type){
    MSG_SERVER2SERVER ping = *(MSG_SERVER2SERVER*)(received);
    int receiver = ping.server_ipc_num;
    ping.server_ipc_num = MSG_RECEIVER;
    msgsnd(receiver, &ping, _size(MSG_SERVER2SERVER), 0);
}

int await_server_response(int node_server_id){
    MSG_SERVER2SERVER pong;
    MSG_SERVER2SERVER ping;
    ping.type = SERVER2SERVER;
    ping.server_ipc_num = MSG_RECEIVER;
    msgsnd(node_server_id, &ping, _size(MSG_SERVER2SERVER), 0);
    clock_t start = clock();
    while (1) {
        if (msgrcv(MSG_RECEIVER, &pong, _size(MSG_SERVER2SERVER), SERVER2SERVER, IPC_NOWAIT) != -1) {
            return TRUE; 
        } else if ( ((double)(clock()-start)*1000)/CLOCKS_PER_SEC > 500){
            return FALSE;
        }
    }
}

MSG_USERS_LIST* get_list(int req_type){
    MSG_USERS_LIST* result = (MSG_USERS_LIST*)malloc(sizeof(MSG_USERS_LIST));
lock_repo();
    int i,j;
    if ( req_type == USERS_LIST) {
        for (i=0, j=0; i<REPO_SIZE; ++i) {
            if (SHM_USER_SERVER_ADRESS[i].server_id != -1) {
                strcpy(result->users[j], SHM_USER_SERVER_ADRESS[i].user_name);
                ++j;
            }
        }
        result->type = USERS_LIST_TYPE;
    } else if ( req_type == ROOMS_LIST) {
        for (i=0, j=0; i<REPO_SIZE; ++i) {
            if (SHM_ROOM_SERVER_ADRESS[i].server_id != -1) {
                strcpy(result->users[j], SHM_ROOM_SERVER_ADRESS[i].room_name);
                ++j;
            }
        }
        result->type = ROOMS_LIST_TYPE;
    }
unlock_repo();
    return result;
}

int get_user_id(const char* username){
    int i;
    for (i=0; i<MAX_USERS_NUMBER; ++i) {
        if ( !strcmp(LOCAL_REPO[i].user_name, username) ) {
            return LOCAL_REPO[i].client_id;
        }
    }
    return -1;
}

time_t get_user_hbtime(const char* username){
    int i;
    for (i=0; i<MAX_USERS_NUMBER; ++i) {
        if ( !strcmp(LOCAL_REPO[i].user_name, username) ) {
            return LOCAL_REPO[i].hb_send_time;
        }
    }
    return time(0);
}


int create_room(const char* roomname){
lock_repo();
    int return_flag = FULL;
    int i;
    for (i=0; i<REPO_SIZE; ++i) {
        if (SHM_ROOM_SERVER_ADRESS[i].server_id == -1) {
            SHM_ROOM_SERVER_ADRESS[i].server_id = MSG_RECEIVER;
            strcpy(SHM_ROOM_SERVER_ADRESS[i].room_name, roomname);
            return_flag = SUCCESS;
            log_data("ROOM %s CREATED", roomname);
            break;
        }
    }
unlock_repo();
    if (return_flag == FULL) {
        log_data("ROOM %s COULDNT BE CREATED, NO SPACE", roomname);
    }
    return return_flag;
}

int delete_room(const char* roomname){
lock_repo();
    int return_flag = FAIL;
    int i;
    for (i=0; i<REPO_SIZE; ++i) {
        if (SHM_ROOM_SERVER_ADRESS[i].server_id == MSG_RECEIVER && !strcmp(SHM_ROOM_SERVER_ADRESS[i].room_name, roomname)) {
            SHM_ROOM_SERVER_ADRESS[i].server_id = -1;
            strcpy(SHM_ROOM_SERVER_ADRESS[i].room_name, "");
            log_data("ROOM %s DELETED", roomname);
            return_flag = SUCCESS;
            break;
        }
    }
unlock_repo();
    if (return_flag == FAIL) {
        log_data("ROOM %s COULDNT BE DELETED, NOT FOUND", roomname);
    }
    return return_flag;
}

int count_people_in_room(const char* roomname){
    int i;
    int result =0;
    for(i=0; i<MAX_USERS_NUMBER; ++i){
        if (!strcmp(LOCAL_REPO[i].room_name, roomname)) {
            result++;
        }
    }
    return result;
}

int change_room(MSG_ROOM room_action, int client_id){
    
lock_repo();
    int i;
    int room_exists_on_server = FALSE;
    for(i=0; i<REPO_SIZE; ++i){
        if( !strcmp(SHM_ROOM_SERVER_ADRESS[i].room_name, room_action.room_name) &&
           SHM_ROOM_SERVER_ADRESS[i].server_id == MSG_RECEIVER){
            room_exists_on_server = TRUE;
            break;
        }
    }
unlock_repo();
    
    if (!room_exists_on_server) {
        int return_flag = create_room(room_action.room_name);
        if (return_flag == FULL) {
            return FULL;
        }
    }
    
    char* old_room = change_users_room_in_local_repo(client_id, room_action.room_name);
    if( count_people_in_room(old_room) == 0){
        delete_room(old_room);
    }
    
    log_data("USER %s CHANGED ROOM FROM  %s TO %s", room_action.user_name, old_room, room_action.room_name);
    free(old_room);
    
    return SUCCESS;
}
void kick_user(const char* username, int client_id){
    MSG_LOGIN user;
    user.type = LOGOUT;
    user.ipc_num = client_id;
    strcpy(user.username, username);
    unregister_user(user);
    remove_user_from_local_repo(client_id);
}


void check_heartbeat_table(){
    int i;
    for (i=0; i<MAX_USERS_NUMBER; ++i) {
        if ( (LOCAL_REPO[i].client_id != -1) && ((time(0) - LOCAL_REPO[i].hb_send_time) > 3) ) {
            kick_user(LOCAL_REPO[i].user_name, LOCAL_REPO[i].client_id);
        }
    }
}


void send_heartbeats_to_clients(){
    int i;
    MSG_RESPONSE hb;
    hb.type = RESPONSE;
    hb.response_type = PING;
    strcpy(hb.content, "");
    
    for (i=0; i<MAX_USERS_NUMBER; ++i) {
        if (LOCAL_REPO[i].client_id != -1) {
            LOCAL_REPO[i].hb_send_time = time(0);
            msgsnd(LOCAL_REPO[i].client_id, &hb, _size(MSG_RESPONSE), 0);
        }
    }
}

int check_if_user_exists(const char* username){
    
lock_repo();
    int return_flag = FALSE;
    int i;
    for (i=0; i<REPO_SIZE; ++i) {
        if (!strcmp(SHM_USER_SERVER_ADRESS[i].user_name, username)) {
            return_flag = SHM_USER_SERVER_ADRESS[i].server_id;
            break;
        }
    }
unlock_repo();
    return return_flag;
}

int unregister_user(MSG_LOGIN user){
lock_repo();
    int return_flag = FAIL;
    int i;
    for(i=0; i<REPO_SIZE; ++i){
        if( !strcmp(SHM_USER_SERVER_ADRESS[i].user_name,user.username) ){
            SHM_USER_SERVER_ADRESS[i].server_id = -1;
            strcpy(SHM_USER_SERVER_ADRESS[i].user_name, "");
            return_flag =  SUCCESS;
            log_data("USER %s SIGNED OUT", user.username);
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
            SHM_USER_SERVER_ADRESS[i].server_id = MSG_RECEIVER;
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

void remove_server(int server_id){
lock_repo();
    int i;
    for (i=0; i<REPO_SIZE; ++i) {
        if (SHM_USER_SERVER_ADRESS[i].server_id == server_id) {
            SHM_USER_SERVER_ADRESS[i].server_id = -1;
            strcpy(SHM_USER_SERVER_ADRESS[i].user_name, "");
        }
    }
    for (i=0; i<REPO_SIZE; ++i) {
        if (SHM_ROOM_SERVER_ADRESS[i].server_id == server_id) {
            SHM_ROOM_SERVER_ADRESS[i].server_id = -1;
            strcpy(SHM_ROOM_SERVER_ADRESS[i].room_name, "");
        }
    }
    for (i=0; i<REPO_SIZE; ++i) {
        if (SHM_SERVER_IDS_ADRESS[i] == server_id) {
            SHM_SERVER_IDS_ADRESS[i] = -1;
        }
    }
unlock_repo();
    log_data("SERVER %d DID NOT RESPOND, REMOVING IT FROM REPO", server_id);
}

int is_local_user(const char* username){
    int i;
    for (i=0; i<MAX_USERS_NUMBER; ++i) {
        if ( !strcmp(LOCAL_REPO[i].user_name, username) ) {
            return TRUE;
        }
    }
    return FALSE;
}

char* change_users_room_in_local_repo(int client_id, const char* roomname){
    int i;
    for(i=0; i<MAX_USERS_NUMBER; ++i){
        if(LOCAL_REPO[i].client_id == client_id){
            char* retval = (char*)malloc(ROOM_NAME_MAX_LENGTH*sizeof(char));
            strcpy(retval, LOCAL_REPO[i].room_name);
            strcpy(LOCAL_REPO[i].room_name, roomname);
            return retval;
        }
    }
    return NULL;
}

void init_local_repo(){
    int i;
    for(i=0; i<MAX_USERS_NUMBER; ++i){
        LOCAL_REPO[i].client_id = -1;
        strcpy(LOCAL_REPO[i].room_name, "");
        strcpy(LOCAL_REPO[i].user_name, "");
        LOCAL_REPO[i].hb_send_time = time(0);
    }
}

void add_to_local_repo(int client_id, const char* username, const char* roomname){
    int i;
    for(i=0; i<MAX_USERS_NUMBER; ++i){
        if(LOCAL_REPO[i].client_id == -1){
            LOCAL_REPO[i].client_id = client_id;
            strcpy(LOCAL_REPO[i].user_name, username);
            strcpy(LOCAL_REPO[i].room_name, roomname);
            LOCAL_REPO[i].hb_send_time = time(0);
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
            strcpy(LOCAL_REPO[i].room_name, "");
            LOCAL_REPO[i].hb_send_time = time(0);
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
    create_room("");
    
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
    for(i=0; i<MAX_USERS_NUMBER; ++i) {
        if (LOCAL_REPO[i].client_id != -1){
            MSG_LOGIN user;
            user.ipc_num = LOCAL_REPO[i].client_id;
            strcpy(user.username, LOCAL_REPO[i].user_name);
            unregister_user(user);
        }
    }

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



