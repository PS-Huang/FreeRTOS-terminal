//#include "log.h"
//#include "ff.h"
//#include "stdio.h"
//#include "string.h"
//#include "stdarg.h"
//
//// SD 卡 log 佇列與開關
//QueueHandle_t xLogQueue = NULL;
//bool log_enabled = false;
//
//void shell_log(const char *fmt, ...)
//{
//    if (!log_enabled || xLogQueue == NULL) return;
//
//    LogMsg_t msg;
//    va_list ap;
//    va_start(ap, fmt);
//    vsnprintf(msg.text, LOG_MSG_MAX_LEN, fmt, ap);
//    va_end(ap);
//
//    // 如果滿了就忽略
//    xQueueSend(xLogQueue, &msg, 0);
//}
//
//void LoggerTask(void *argument)
//{
//    FATFS fs;
//    FIL fp;
//    FRESULT res;
//
//    // 掛載 SD 卡
//    while (f_mount(&fs, "", 1) != FR_OK) {
//        vTaskDelay(pdMS_TO_TICKS(1000));
//    }
//
//    // 開檔 (追加)
//    res = f_open(&fp, "log.txt", FA_OPEN_APPEND | FA_WRITE);
//    if (res != FR_OK) {
//        vTaskDelete(NULL);
//    }
//
//    TickType_t lastFlush = xTaskGetTickCount();
//    LogMsg_t msg;
//
//    while (1) {
//        if (xQueueReceive(xLogQueue, &msg, pdMS_TO_TICKS(100)) == pdPASS) {
//            UINT bw;
//            f_write(&fp, msg.text, strlen(msg.text), &bw);
//        }
//
//        if ((xTaskGetTickCount() - lastFlush) > pdMS_TO_TICKS(1000)) {
//            f_sync(&fp);
//            lastFlush = xTaskGetTickCount();
//        }
//    }
//}
