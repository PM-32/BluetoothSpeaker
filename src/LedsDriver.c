#include "esp32-hal-gpio.h"

#include "LedsDriver.h"

//! \brief Инициализация светодиода
void LedsDriver_Init(void)
{
    // Настройка пина светодиода как выход
    pinMode(BLUETOOTH_STATUS_LED_PIN, OUTPUT);
}

//! \brief Установка состояния светодиода
//! \param[in] ledPin - пин светодиода
//! \param[in] ledState - состояние светодиода
void LedsDriver_SetLedState(uint8_t ledPin, LedState ledState)
{
    digitalWrite(ledPin, ledState);
}

//! \brief Смена состояния светодиода
//! \param[in] ledPin - пин светодиода
void LedsDriver_ToggleLed(uint8_t ledPin)
{
    // Если светодиод выключен
    if (LED_OFF == digitalRead(ledPin))
    {
        // Включение светодиода
        digitalWrite(ledPin, LED_ON);
    }
    else // Если светодиод включен
    {
        // Выключение светодиода
        digitalWrite(ledPin, LED_OFF);
    }
}