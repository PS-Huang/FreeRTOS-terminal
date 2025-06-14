#ifndef __LOG_H__
#define __LOG_H__

#include "FreeRTOS.h"
#include "queue.h"
#include <stdbool.h>

#define LOG_MSG_MAX_LEN 128

typedef struct {
    char text[LOG_MSG_MAX_LEN];
} LogMsg_t;

// 全域佇列 handle
extern QueueHandle_t xLogQueue;

// log 開關控制（由 shell task 控制）
extern bool log_enabled;

// 任務與函式
void LoggerTask(void *argument);
void shell_log(const char *fmt, ...);

#endif

