#include "esp32-hal-gpio.h"

#include "LedsDriver.h"

//! \brief Инициализация светодиода
void LedsDriver_Init(void)
{
    // Настройка пина светодиода как выход
    pinMode(BLUETOOTH_STATUS_LED_PIN, OUTPUT);
}

//! \brief Установка состояния светодиода
//! \param[in] led - номер светодиода
//! \param[in] ledState - состояние светодиода
void LedsDriver_SetLedState(uint8_t ledPin, LedState ledState)
{
    digitalWrite(ledPin, ledState);
}