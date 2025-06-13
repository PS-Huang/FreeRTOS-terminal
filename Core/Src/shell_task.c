#include "main.h"
#include "string.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "shell_task.h"

extern UART_HandleTypeDef huart2;

#define MAX_CMD_LEN 128

typedef struct {
    const char* name;
    int argc_min;
    void (*handler)(int argc, char** argv);
    const char* help;
} CLI_Command;

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

// 指令列表
static void cmd_help(int argc, char** argv);
static void cmd_echo(int argc, char** argv);
static void cmd_status(int argc, char** argv);
// 新增 cmd_led, cmd_measure...

static const CLI_Command cli_commands[] = {
    {"help",     0, cmd_help,   "help                Show help"},
    {"echo",     1, cmd_echo,   "echo <text>         Echo text"},
	{"status",   0, cmd_status, "status              Show task status"}
    // {"led",      ...},
    // {"measure",  ...},
    // etc.
};

#define CMD_COUNT (sizeof(cli_commands)/sizeof(cli_commands[0]))

// 實作每個指令
static void shell_write(const char* s) {
    HAL_UART_Transmit(&huart2, (uint8_t*)s, strlen(s), HAL_MAX_DELAY);
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


// Parser function
static void parse_and_execute(char* line) {
    char* argv[10];
    int argc = 0;
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
            } else if (idx < (int)sizeof(buf) - 1 && ch >= 32 && ch <= 126) {
                buf[idx++] = ch;
                HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
            }
        }
    }
}
