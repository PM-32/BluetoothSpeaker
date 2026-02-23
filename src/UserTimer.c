#include <stdbool.h>

#include "esp32-hal-timer.h"

#include "Callbacks.h"
#include "UserTimer.h"

#define TIMER_PERIOD_MKS    100             //!< Период таймера в мкс

static hw_timer_t *pIntervalTimer = NULL;   //!< Указатель на таймер для замера интервалов
static volatile uint32_t counterTime = 0;   //!< Счетчик времени работы программы

//! \brief Инициализация таймера для отсчета интервалов
void UserTimer_InitTimer(void)
{
    // Создание таймера 0.
    // Делитель 80 (1 тик = 1 мкс), счет вверх
    pIntervalTimer = timerBegin(0, 80, true);
    
    // Получение адреса коллбэк-функции прерывания по переполнению таймера
    void *pTim0PeriodElapsedCallback = Callbacks_GetTim0PeriodElapsedCallbackFunctionPointer();

    // Привязка обработчика прерывания к таймеру 0
    timerAttachInterrupt(pIntervalTimer, pTim0PeriodElapsedCallback, true);
    
    // Установка периода таймера и включение автоперезагрузки
    timerAlarmWrite(pIntervalTimer, TIMER_PERIOD_MKS, true);
}

//! \brief Запуск таймера 0
void UserTimer_StartTim0(void)
{
    timerAlarmEnable(pIntervalTimer);
}

//! \brief Инкремент счетчика времени работы программы
void UserTimer_IncrementCounter(void)
{
    counterTime++;
}

//! \brief Получение текущего значения
//!        счетчика времени работы программы
uint32_t UserTimer_GetCounterTime(void)
{
    return counterTime;
}