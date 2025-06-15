#ifndef SHELL_TASK_H
#define SHELL_TASK_H

#include "FreeRTOS.h"
#include "queue.h"

#define LOG_BUF_LEN 128

typedef struct {
    char text[LOG_BUF_LEN];
} LogMsg_t;

extern QueueHandle_t xLogQueue;

void ShellTask(void *argument);
void log_printf(const char* fmt, ...);
void LoggerTask(void *param);

#endif
