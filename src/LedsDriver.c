#include "esp32-hal-gpio.h"
#include "soc/gpio_reg.h"

#include "CommonFunctions.h"
#include "LedsDriver.h"
#include "UserTimer.h"

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
    // Установка состояния пина
    CommonFunctions_GpioSetState(ledPin, ledState);
}

//! \brief Смена состояния светодиода
//! \param[in] ledPin - пин светодиода
void LedsDriver_ToggleLed(uint8_t ledPin)
{
    // Смена состояния пина
    CommonFunctions_GpioToggleState(ledPin);
}