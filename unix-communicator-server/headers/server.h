//
//  server.h
//  unix-communicator-server
//
//  Created by Maciej Żurad on 3/3/13.
//  Copyright (c) 2013 Maciej Żurad. All rights reserved.
//

#ifndef unix_communicator_server_server_h
#define unix_communicator_server_server_h

enum USER_REGISTER_CODES {SUCCESS, EXISTS, FULL, FAIL};

typedef struct {
    char user_name[USER_NAME_MAX_LENGTH];
    char room_name[ROOM_NAME_MAX_LENGTH];
    int client_id;
} client;

typedef void (*fnc_handler)(void*, int);


//globals
int SHM_SERVER_IDS;
int SHM_USER_SERVER;
int SHM_ROOM_SERVER;

int SEM_SERVER_IDS;
int SEM_USER_SERVER;
int SEM_ROOM_SERVER;
int SEM_LOGFILE;

int* SHM_SERVER_IDS_ADRESS;
ROOM_SERVER* SHM_ROOM_SERVER_ADRESS;
USER_SERVER* SHM_USER_SERVER_ADRESS;

const char* LOG_FILENAME = "chat.log";

int MSG_RECEIVER;
client LOCAL_REPO[MAX_USERS_NUMBER];

void init_server();
void init_repo();
void init_sems();

void lock_repo();
void unlock_repo();

void lock_log();
void unlock_log();

void sem_up();
void sem_down();

void register_new_server();

void clean();
void clean_repo();
void release_resources();

void log_data(const char* text, ...);

void receive_msg(void* received, size_t msg_size, int msg_type, fnc_handler handler);

void handle_login(void* received, int msg_type);
void handle_logout(void* received, int msg_type);
void handle_request(void* received, int msg_type);
void handle_message(void* received, int msg_type);
void handle_room_action(void* received, int msg_type);
void handle_server_heartbeat(void* received, int msg_type);

int register_new_user(MSG_LOGIN user);
int unregister_user(MSG_LOGIN user);

void init_local_repo();
void add_to_local_repo(int client_id, const char* username, const char* roomname);
void remove_user_from_local_repo(int client_id);
char* change_users_room_in_local_repo(int client_id, const char* roomname);

int create_room(const char* roomname);
int delete_room(const char* roomname);

int get_user_id(const char* username);
#endif
