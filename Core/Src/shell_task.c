#include "main.h"
#include "string.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "shell_task.h"
#include "stdbool.h"
#include "queue.h"
#include "stdarg.h"

extern UART_HandleTypeDef huart2;

#define MAX_CMD_LEN 128

#define LOG_LINE_MAX     96
#define LOG_RING_SIZE    128
#define EXTERNAL_TASK_COUNT (sizeof(external_tasks)/sizeof(external_tasks[0]))
#define MAX_USER_TASKS   16

typedef struct {
    const char* name;
    int argc_min;
    void (*handler)(int argc, char** argv);
    const char* help;
} CLI_Command;

typedef struct {
    TaskHandle_t handle;
    char         name[16];
    uint16_t     stackSize;        /* 建立任務時傳入的 usStackDepth  */
} TaskInfo_t;

typedef struct {
    const char* name;
    TaskFunction_t task_entry;
    uint16_t stack_size;
    UBaseType_t priority;
} ExternalTaskEntry;

static TaskInfo_t g_taskTable[MAX_USER_TASKS];
static UBaseType_t g_taskCount = 0;

static const char* shell_banner =
		"\r\n"
		"=========================================================================\r\n"
		"* ______              _____ _          _ _ _____ _______ ____   _____   *\r\n"
		"* |  ____|            / ____| |        | | |  __ \\__   __/ __ \\ / ____| *\r\n"
		"* | |__ _ __ ___  ___| (___ | |__   ___| | | |__) | | | | |  | | (___   *\r\n"
		"* |  __| '__/ _ \\/ _ \\\\___ \\| '_ \\ / _ \\ | |  _  /  | | | |  | |\\___ \\  *\r\n"
		"* | |  | | |  __/  __/____) | | | |  __/ | | | \\ \\  | | | |__| |____) | *\r\n"
		"* |_|  |_|  \\___|\\___|_____/|_| |_|\\___|_|_|_|  \\_\\ |_|  \\____/|_____/  *\r\n"
		"=========================================================================\r\n"
		"\r\n"
		"Welcome to FreeShellRTOS !!! \r\n"
		"Type 'help' to see available commands.\r\n";

QueueHandle_t xLogQueue     = NULL;            // queue handle
static LogMsg_t      g_ring[LOG_RING_SIZE];
static uint16_t      g_head = 0;                      // 下一寫入位置
static uint16_t      g_count = 0;                     // 目前已存行數
static bool          g_logEnabled = false;            // log on/off

// 指令列表
static void cmd_help(int argc, char** argv);
static void cmd_echo(int argc, char** argv);
static void cmd_status(int argc, char** argv);
static void cmd_log(int argc, char** argv);
static void cmd_uptime(int argc, char** argv);
static void cmd_mem(int argc, char** argv);
static void cmd_ext(int argc, char** argv);
static void cmd_led(int argc, char** argv);
// 新增 cmd_led, cmd_measure...

static const CLI_Command cli_commands[] = {
    {"help",     0, cmd_help,   	"help                    Show help"},
    {"echo",     1, cmd_echo,   	"echo <text>             Echo text"},
	{"status",   0, cmd_status, 	"status                  Show task status"},
	{"uptime",   0, cmd_uptime, 	"uptime                  Show system uptime"},
	{"log",      1, cmd_log,    	"log on | off | dump     logger control"},
    {"mem",      0, cmd_mem,    	"mem                     Show memory and stack usage"},
    {"run", 	 2, cmd_ext, 		"run task <name>         Enter run task ext1, and the task ExternalTask1 will be created"},
    {"led",      1, cmd_led,        "led -<color> [on|off]   Control LED (color: red, blue, green, orange)"}
    // {"measure",  ...},
    // etc.
};

#define CMD_COUNT (sizeof(cli_commands)/sizeof(cli_commands[0]))

// 將吃到的字元返還
static void shell_write(const char* s) {
    HAL_UART_Transmit(&huart2, (uint8_t*)s, strlen(s), HAL_MAX_DELAY);

    if (g_logEnabled && xLogQueue) {
		LogMsg_t m;
		strncpy(m.text, s, LOG_LINE_MAX - 1);
		m.text[LOG_LINE_MAX-1] = '\0';
		xQueueSend(xLogQueue, &m, 0);
	}
}

void log_printf(const char *fmt, ...)
{
    if (!g_logEnabled || !xLogQueue) return;
    LogMsg_t m;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m.text, LOG_LINE_MAX, fmt, ap);
    va_end(ap);
    xQueueSend(xLogQueue, &m, 0);
}

void LoggerTask(void *param)
{
    LogMsg_t m;
    for (;;) {
        if (xQueueReceive(xLogQueue, &m, portMAX_DELAY) == pdPASS) {
            // 寫進環形緩衝區
            memcpy(&g_ring[g_head], &m, sizeof(LogMsg_t));
            g_head  = (g_head + 1) % LOG_RING_SIZE;
            if (g_count < LOG_RING_SIZE) g_count++;
        }
    }
}

void RegisterUserTask(TaskHandle_t h, const char *name, uint16_t stackSize)
{
    if (g_taskCount < MAX_USER_TASKS) {
        g_taskTable[g_taskCount].handle    = h;
        strncpy(g_taskTable[g_taskCount].name, name, sizeof(g_taskTable[g_taskCount].name)-1);
        g_taskTable[g_taskCount].stackSize = stackSize;
        g_taskCount++;
    }
}

static uint16_t findStackSize(TaskHandle_t h)
{
    for (UBaseType_t i = 0; i < g_taskCount; i++) {
        if (g_taskTable[i].handle == h) {
            return g_taskTable[i].stackSize;    // user-defined task
        }
    }
    return 0;     // fallback
}

static void cmd_help(int argc, char** argv) {
    shell_write("\r\nAvailable commands:\r\n");
    for (int i = 0; i < CMD_COUNT; i++) {
        shell_write("  ");
        shell_write(cli_commands[i].help);
        shell_write("\r\n");
    }
}

static void cmd_echo(int argc, char** argv) {
    // 從 argv[1] 開始輸出所有參數
    for (int i = 1; i < argc; i++) {
        shell_write(argv[i]);
        if (i < argc - 1) shell_write(" ");
    }
    shell_write("\r\n");
}

static void cmd_status(int argc, char** argv) {
	uint8_t InfoBuffer[1000];
	uint8_t RunTimeBuffer[1000];
    vTaskList((char *)&InfoBuffer);
    vTaskGetRunTimeStats((char *)&RunTimeBuffer);
    shell_write("Name          State     Pr     Stack  TaskNum\r\n");
    shell_write("---------------------------------------------\r\n");
    shell_write(InfoBuffer);
    shell_write("\r\n");
    shell_write("Name            Count        Utlization\r\n");
    shell_write("---------------------------------------\r\n");
    shell_write(RunTimeBuffer);
}

static void cmd_uptime(int argc, char** argv) {
    TickType_t ticks = xTaskGetTickCount(); // 取得系統經過的 ticks
    uint32_t ms = ticks * portTICK_PERIOD_MS;

    uint32_t seconds = ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours   = minutes / 60;

    seconds = seconds % 60;
    minutes = minutes % 60;

    char line[64];
    snprintf(line, sizeof(line), "Uptime: %02lu:%02lu:%02lu (%lu ms)\r\n",
             hours, minutes, seconds, ms);
    shell_write(line);
}

static void cmd_log(int argc, char **argv)
{
    if (argc < 2) { shell_write("Usage: log on|off|dump\r\n"); return; }

    if (strcmp(argv[1], "on") == 0) {
        g_logEnabled = true;
        shell_write("Log ON\r\n");
    } else if (strcmp(argv[1], "off") == 0) {
        g_logEnabled = false;
        shell_write("Log OFF\r\n");
    } else if (strcmp(argv[1], "dump") == 0) {
        shell_write("=== Log Dump Start ===\r\n");
        uint16_t start = (g_head + LOG_RING_SIZE - g_count) % LOG_RING_SIZE;
        for (uint16_t i = 0; i < g_count; i++) {
            uint16_t idx = (start + i) % LOG_RING_SIZE;
            shell_write(g_ring[idx].text);
        }
        shell_write("===  Log Dump End  ===\r\n");
    } else {
        shell_write("Usage: log on | off | dump\r\n");
    }
}

static void cmd_mem(int argc, char **argv)
{
    /* 先列出 Heap 資訊 … */
	char line[128];
	size_t heap_free = xPortGetFreeHeapSize();
	size_t heap_min = xPortGetMinimumEverFreeHeapSize();
	shell_write("\r\nMemory Summary:\r\n\r\n");

	snprintf(line, sizeof(line), "    Heap Total Free      : %lu bytes\r\n", (unsigned long)heap_free);
	shell_write(line);
	snprintf(line, sizeof(line), "    Heap Min Ever Free   : %lu bytes\r\n\r\n", (unsigned long)heap_min);
	shell_write(line);


    UBaseType_t n = uxTaskGetNumberOfTasks();
    TaskStatus_t *tsArray = pvPortMalloc(n * sizeof(TaskStatus_t));
    n = uxTaskGetSystemState(tsArray, n, NULL);

    shell_write("\r\nTask            Stack  Used Free            %%\r\n"
                "--------------- ------ ---- ---- -------------\r\n");

    char buf[96];
    for (UBaseType_t i = 0; i < n; i++) {
        TaskStatus_t *t = &tsArray[i];
        uint16_t total = findStackSize(t->xHandle);
        uint16_t free  = t->usStackHighWaterMark;
        uint16_t used  = total - free;
        uint8_t  pct   = (used * 100UL) / total;

        if(total == 0){
        	snprintf(buf, sizeof(buf), "%-15s %5s  %4s %4u %13s\r\n",
        	        	                 t->pcTaskName, "N/A","N/A", free, "system task");
        	        	        shell_write(buf);
        }
        else{
        	 snprintf(buf, sizeof(buf), "%-15s %5u  %4u %4u %12u%%\r\n",
        	                 t->pcTaskName, total, used, free, pct);
        	        shell_write(buf);
        }
    }
    vPortFree(tsArray);
}

// Parser function
static void parse_and_execute(char* line) {
    char* argv[10];
    int argc = 0;

    log_printf(">>> %s\r\n", line);

    char* p = strtok(line, " ");
    while (p && argc < 10) {
        argv[argc++] = p;
        p = strtok(NULL, " ");
    }
    if (argc == 0) return;

    for (int i = 0; i < CMD_COUNT; i++) {
        if (strcmp(argv[0], cli_commands[i].name) == 0) {
            if (argc - 1 < cli_commands[i].argc_min) {
                shell_write("Usage: ");
                shell_write(cli_commands[i].help);
                shell_write("\r\n");
            } else {
                cli_commands[i].handler(argc, argv);
            }
            return;
        }
    }
    shell_write("Unknown command: ");
    shell_write(argv[0]);
    shell_write("\r\n");
}

// Shell Task
void ShellTask(void* argument) {
    static const char* banner =
       "FreeShellRTOS:/$ ";  // 已經在這前面印 banner 的程式

    char buf[128];
    int idx = 0;
    char ch;

    shell_write(shell_banner);
    shell_write(banner);

    while (1) {
        if (HAL_UART_Receive(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY) == HAL_OK) {
            if (ch == '\r') {
                buf[idx] = '\0';
                shell_write("\r\n");
                parse_and_execute(buf);
                shell_write("FreeShellRTOS:/$ ");
                idx = 0;
            } else if (ch == '\b' || ch == 0x7F) { 
                if (idx > 0) {
                    idx--;
                    shell_write("\b \b"); // 在終端機上刪除字元邏輯: 後退, 覆蓋為空格, 再後退
                }
            } else if (idx < (int)sizeof(buf) - 1 && ch >= 32 && ch <= 126) {
                buf[idx++] = ch;
                HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
            }
        }
    }
}

TaskHandle_t ext1_handle = NULL;

void ExternalTask1(void* arg) {
    shell_write("[ext1] Task started\r\n");
    while (1) {
        // 模擬任務行為
        shell_write("[ext1] Doing some work...\r\n");
        vTaskDelay(pdMS_TO_TICKS(2000));  // 模擬 2 秒工作

        // 進入睡眠，等待下一次 run 指令叫醒
        shell_write("[ext1] Sleeping...\r\n");
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        shell_write("[ext1] Woke up again!\r\n");
    }
}



static const ExternalTaskEntry external_tasks[] = {
    { "ext1", ExternalTask1, 128, 2 },
    // 你可以繼續擴充更多
};

extern TaskHandle_t ext1_handle;

static void cmd_ext(int argc, char** argv) {
    if (argc < 2) {
        shell_write("Usage: run <task_name>\r\n");
        return;
    }

    if (strcmp(argv[2], "ext1") == 0) {
        if (ext1_handle == NULL) {
            // 還沒創建，先建立它
            if (xTaskCreate(ExternalTask1, "ext1", 256, NULL, 2, &ext1_handle) == pdPASS) {
            	RegisterUserTask(ext1_handle, "Logger", 256);
                shell_write("Task ext1 created and running...\r\n");
            } else {
                shell_write("Failed to create task ext1\r\n");
            }
        } else {
            // 已建立，只是睡著了 → 叫醒
            shell_write("Task ext1 resumed...\r\n");
            xTaskNotifyGive(ext1_handle);
        }
        return;
    }

    shell_write("Unknown task name\r\n");
}

static void control_single_led(const char* color_name, uint16_t GPIO_Pin, bool turn_on) {
    HAL_GPIO_WritePin(GPIOD, GPIO_Pin, turn_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    char msg[64];
    snprintf(msg, sizeof(msg),"%s LED turned %s.\r\n", color_name, turn_on ? "ON" : "OFF");
    shell_write(msg);
}

static void cmd_led(int argc, char** argv) {
    bool turn_on = true;
    if (argc < 2) {
        shell_write("Usage: led -<color> [on|off]\r\n");
        shell_write("Colors: -red, -blue, -green, -orange\r\n");
        return;
    }

    if (argc == 3) {
        if (strcmp(argv[2], "on") == 0) {
            turn_on = true;
        } else if (strcmp(argv[2], "off") == 0) {
            turn_on = false;
        } else {
            shell_write("Invalid state argument. Use 'on' or 'off'.\r\n");
            return;
        }
    } else if (argc > 3) {
        shell_write("Too many arguments.\r\n");
        shell_write("Usage: led -<color> [on|off]\r\n");
        return;
    }

    if (strcmp(argv[1], "-green") == 0) {
        control_single_led("Green", GPIO_PIN_12, turn_on);
    } else if (strcmp(argv[1], "-orange") == 0) {
        control_single_led("Orange", GPIO_PIN_13, turn_on);
    } else if (strcmp(argv[1], "-red") == 0) {
        control_single_led("Red", GPIO_PIN_14, turn_on);
    } else if (strcmp(argv[1], "-blue") == 0) {
        control_single_led("Blue", GPIO_PIN_15, turn_on);
    } else {
        shell_write("Unknown LED color. Use -green, -orange, -red, or -blue.\r\n");
    }
}
