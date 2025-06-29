//
// Created by angaros on 3/10/2025.
//

#ifndef SERVER_CONSTANTS_H
#define SERVER_CONSTANTS_H

    #include <stdbool.h>


    #ifndef CLIENT_MESSAGES_DIR
        #define CLIENT_MESSAGES_DIR "client_messages" // Client log folder
    #endif // CLIENT_MESSAGES_DIR

    #define DEFAULT_SERVER_PORT "21000"
    #define DEFAULT_SERVER_ADDR "127.0.0.1"
    #define BACKUP_SERVER_ADDR "127.0.0.1"
    #define DEFAULT_WEB_ADDR "0.0.0.0" // Listen on all incoming
    #define DEFAULT_WEB_PORT "8000"
    #define MAX_CLIENTS 32

    extern bool enable_cli; // Global CLI variable

#endif // SERVER_CONSTANTS_H