#include <stdbool.h>

#include <esp_task_wdt.h>

#include "WatchDog.h"

#define WATCH_DOG_TIMEOUT_SECONDS     60          //!< Таймаут сторожевого таймера в секундах

//! \brief Состояние инициализации модуля сторожевого таймера
typedef enum
{
    WATCH_DOG_NOT_INITIALIZED = 0,     //!< Модуль сторожевого таймера не инициализирован
    WATCH_DOG_INITIALIZED              //!< Модуль сторожевого таймера инициализирован
} WatchDogInitState;

static WatchDogInitState watchDogInitState = WATCH_DOG_NOT_INITIALIZED;     //!< Состояние инициализации модуля сторожевого таймера

//! \brief Инициализация сторожевого таймера
void WatchDog_Init(void)
{
    // Инициализация сторожевого таймера задач
    esp_task_wdt_init(WATCH_DOG_TIMEOUT_SECONDS, true);
    
    // Подключение текущей задачи к сторожевому таймеру
    esp_task_wdt_add(NULL);
    
    // Установка статуса - модуль инициализирован
    watchDogInitState = WATCH_DOG_INITIALIZED;
}

//! \brief Сброс сторожевого таймера
void WatchDog_Reset(void)
{
    // Выход, если модуль не инициализирован
    if (WATCH_DOG_NOT_INITIALIZED == watchDogInitState)
    {
        return;
    }
    
    // Сброс сторожевого таймера
    esp_task_wdt_reset();
}