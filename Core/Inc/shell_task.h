#ifndef SHELL_TASK_H
#define SHELL_TASK_H

#include "FreeRTOS.h"
//#include "task.h"  // 為了 TaskHandle_t
#include "queue.h"

#define LOG_BUF_LEN 128
#define MAX_COMMAND_LENGTH  128 // 最大長度
#define COMMAND_HISTORY_SIZE 10 // 最多儲存10條命令

typedef struct {
    char text[LOG_BUF_LEN];
} LogMsg_t;

typedef struct {
    char commands[COMMAND_HISTORY_SIZE][MAX_COMMAND_LENGTH]; // 儲存命令的二維陣列
    int head;
    int tail;
    int count;
} CommandHistory_t;

extern QueueHandle_t xLogQueue;

void ShellTask(void *argument);
void log_printf(const char* fmt, ...);
void LoggerTask(void *param);
//void RegisterUserTask(TaskHandle_t h, const char *name, uint16_t stackSize);

//void history_init(CommandHistory_t *history);
//void history_add(CommandHistory_t *history, const char *command);
//const char* history_get(CommandHistory_t *history, int index);

#endif
