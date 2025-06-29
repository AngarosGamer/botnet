//
// Created by angaros on 3/10/2025.
//

#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdbool.h>

enum LogCode {
    LOG_ERROR = 0, // Application errors
    LOG_WARNING = 1, // Potential error incoming
    LOG_INFO = 2, // Informational state (start, pause, ...)
    LOG_DEBUG = 3, // Just for debugging
  };

enum LogType {
    LOG_TO_ALL = 0, // Logs to all connected logging mechanisms
    LOG_TO_CONSOLE = 1, // Only a console log
    LOG_TO_FILE = 2, // Only a file log
    LOG_TO_NONE = 3 // Do not log
};

enum LogLevel {
  LOG_LEVEL_ERROR = 0,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_INFO,
  LOG_LEVEL_DEBUG,
  LOG_SUPPRESS_ERRORS
};

extern enum LogLevel current_log_level; // Global variable
extern bool suppress_errors_flag;

void get_timestamp(char *buffer, size_t bufsize);
void output_log(const char *fmt, const enum LogCode code, const enum LogType type, ...);

#endif //LOGGING_H
