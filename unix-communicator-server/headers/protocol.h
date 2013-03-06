//
//  protocol.h
//  unix-communicator-client
//
//  Created by Maciej Żurad on 2/28/13.
//  Copyright (c) 2013 Maciej Żurad. All rights reserved.
//

#include <stdlib.h>
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
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#ifndef unix_communicator_client_protocol_h
#define unix_communicator_client_protocol_h

#define ROOM_NAME_MAX_LENGTH 10
#define USER_NAME_MAX_LENGTH 10

#define RESPONSE_LENGTH 50

#define REPO_SIZE MAX_SERVERS_NUMBER*MAX_USERS_NUMBER

#define MAX_SERVERS_NUMBER 3 // supposed to be 15, cant be due to OSX constraints
#define MAX_USERS_NUMBER 20
#define MAX_MSG_LENGTH 256

#define SEM_SERVER_IDS_KEY 35
#define SEM_USER_SERVER_KEY 36
#define SEM_ROOM_SERVER_KEY 37
#define SEM_LOGFILE_KEY 38

#define SHM_SERVER_IDS_KEY 15
#define SHM_USER_SERVER_KEY 20
#define SHM_ROOM_SERVER_KEY 25

#define TRUE 1
#define FALSE 0

#define _size(X) (sizeof(X)-sizeof(long))

// tie-up of user and server its on
typedef struct {
    char user_name[USER_NAME_MAX_LENGTH];
    int server_id;
} USER_SERVER;

// tie-up of channel and server its on[ROOM_NAME_MAX_LENGTH]
typedef struct {
    char room_name[ROOM_NAME_MAX_LENGTH];
    int server_id;
} ROOM_SERVER;

enum MSG_TYPE {LOGIN=1, RESPONSE, LOGOUT, REQUEST, MESSAGE, ROOM, SERVER2SERVER, USERS_LIST_TYPE, ROOMS_LIST_TYPE, ROOM_USERS_LIST_TYPE};

// login request struct
typedef struct {
    long type;
    char username[USER_NAME_MAX_LENGTH];
    int ipc_num;
} MSG_LOGIN;


enum RESPONSE_TYPE {LOGIN_SUCCESS, LOGIN_FAILED, LOGOUT_SUCCESS, LOGOUT_FAILED, MSG_SEND, MSG_NOT_SEND, ENTERED_ROOM_SUCCESS, ENTERED_ROOM_FAILED, CHANGE_ROOM_SUCCESS, CHANGE_ROOM_FAILED, LEAVE_ROOM_SUCCESS, LEAVE_ROOM_FAILED, PING};

typedef struct {
    long type;
    int response_type;
    char content[RESPONSE_LENGTH];
} MSG_RESPONSE;

enum REQUEST_TYPE{USERS_LIST, ROOMS_LIST, ROOM_USERS_LIST, PONG};

typedef struct {
    long type;
    int request_type;
    char user_name[USER_NAME_MAX_LENGTH];
} MSG_REQUEST;

typedef struct{
    long type;
    char users[MAX_SERVERS_NUMBER*MAX_USERS_NUMBER][USER_NAME_MAX_LENGTH];
}MSG_USERS_LIST;

enum CHAT_MESSAGE_TYPE {PUBLIC, PRIVATE};

typedef struct{
    long type;
    int msg_type;
    char send_time[6];
    char sender[USER_NAME_MAX_LENGTH];
    char receiver[USER_NAME_MAX_LENGTH];
    char message[MAX_MSG_LENGTH];
} MSG_CHAT_MESSAGE;

enum ROOM_OPERATION_TYPE {ENTER_ROOM, LEAVE_ROOM, CHANGE_ROOM};

typedef struct {
    long type;
    int operation_type;
    char user_name[USER_NAME_MAX_LENGTH];
    char room_name[ROOM_NAME_MAX_LENGTH];
} MSG_ROOM;

typedef struct{
    long type;
    int server_ipc_num;
} MSG_SERVER2SERVER;



#endif
