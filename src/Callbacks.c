#include <stdint.h>
#include <stdbool.h>

#include "esp_attr.h"

#include "ButtonsDriver.h"
#include "Callbacks.h"
#include "UserTimer.h"

//! \brief Коллбэк-функция прерывания по переполнению таймера (период 100 мкс)
void IRAM_ATTR TIM0_PeriodElapsedCallback(void)
{
    // Инкремент счетчика времени работы программы
    UserTimer_IncrementCounter();

    // Фильтр антидребезга кнопок
    ButtonsDriver_AntibounceFilter();
}

//! \brief Получение адреса коллбэк-функции прерывания по переполнению таймера
//! \return Адрес коллбэк-функции прерывания по переполнению таймера
void * Callbacks_GetTim0PeriodElapsedCallbackFunctionPointer(void)
{
    return (void *) &TIM0_PeriodElapsedCallback;
}