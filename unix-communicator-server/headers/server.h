//
//  server.h
//  unix-communicator-server
//
//  Created by Maciej Żurad on 3/3/13.
//  Copyright (c) 2013 Maciej Żurad. All rights reserved.
//

#ifndef unix_communicator_server_server_h
#define unix_communicator_server_server_h

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
void release_resources();

void log_data(const char* text, ...);

#endif
