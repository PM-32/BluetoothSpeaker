#include "esp32-hal-gpio.h"
#include "soc/gpio_reg.h"

#include "CommonFunctions.h"

#define GPIO_REG_SIZE_IN_BITS       32          //!< Размер регистров чтения/записи в битах
#define MAX_PIN_NUMBER              39          //!< Максимальный номер пина

// Пояснение. Устройство пинов в ESP32.
// Состояния пинов ESP32 содержатся в двух регистрах: GPIO_IN_REG и
// GPIO_IN1_REG. GPIO_IN_REG содержит состояния пинов GPIO_0 - GPIO_31
// (32 бит - [31:0]). GPIO_IN1_REG содержит состояния пинов
// GPIO_32 - GPIO_39 (8 бит - [7:0]). Регистры для записи состояний
// пинов GPIO_OUT_REG и GPIO_OUT1_REG устроены аналогично.

//! \brief Чтение состояния пина
//! \param[in] gpioPin - номер пина
PinState CommonFunctions_GpioGetState(uint8_t gpioPin)
{
    // Битовая маска для выделения пина
    uint32_t pinBitMask = 0;

    // Состояние бита (пина)
    PinState pinState = PIN_RESET;

    // Если номер пина меньше GPIO_REG_SIZE_IN_BITS
    if (gpioPin < GPIO_REG_SIZE_IN_BITS)
    {
        pinBitMask = (uint32_t) (PIN_SET << gpioPin);

        // Чтение состояния пина из регистра GPIO_IN_REG
        pinState = REG_READ(GPIO_IN_REG) & pinBitMask;
    }
    else // Если номер пина не менее GPIO_REG_SIZE_IN_BITS
    {
        // Номер пина не превышает максимальный номер пина
        if (gpioPin <= MAX_PIN_NUMBER)
        {
            pinBitMask = (uint32_t) (PIN_SET << (gpioPin - GPIO_REG_SIZE_IN_BITS));

            // Чтение состояния пина из регистра GPIO_IN1_REG
            pinState = REG_READ(GPIO_IN1_REG) & pinBitMask;
        }
    }

    return pinState;
}

//! \brief Установка состояния пина
//! \param[in] gpioPin - номер пина
//! \param[in] pinState - логический уровень
void CommonFunctions_GpioSetState(uint8_t gpioPin, PinState pinState)
{
    // Битовая маска для выделения пина
    uint32_t pinBitMask = 0;

    // Если номер пина меньше GPIO_REG_SIZE_IN_BITS
    if (gpioPin < GPIO_REG_SIZE_IN_BITS)
    {
        pinBitMask = (uint32_t) (PIN_SET << gpioPin);

        // Если нужно установить лог. "1"
        if (PIN_SET == pinState)
        {
            // Установка бита в регистре GPIO_OUT_REG
            REG_WRITE(GPIO_OUT_REG, REG_READ(GPIO_OUT_REG) | pinBitMask);
        }
        else // Если нужно установить лог. "0"
        {
            // Сброс бита в регистре GPIO_OUT_REG
            REG_WRITE(GPIO_OUT_REG, REG_READ(GPIO_OUT_REG) & (~pinBitMask));
        }
    }
    else // Если номер пина не менее GPIO_REG_SIZE_IN_BITS
    {
        // Номер пина не превышает максимальный номер пина
        if (gpioPin <= MAX_PIN_NUMBER)
        {
            pinBitMask = (uint32_t) (PIN_SET << (gpioPin - GPIO_REG_SIZE_IN_BITS));

            // Если нужно установить лог. "1"
            if (PIN_SET == pinState)
            {
                // Установка бита в регистре GPIO_OUT1_REG
                REG_WRITE(GPIO_OUT1_REG, REG_READ(GPIO_OUT1_REG) | pinBitMask);
            }
            else // Если нужно установить лог. "0"
            {
                // Сброс бита в регистре GPIO_OUT1_REG
                REG_WRITE(GPIO_OUT1_REG, REG_READ(GPIO_OUT1_REG) & (~pinBitMask));
            }
        }
    }
}

//! \brief Смена состояния пина
//! \param[in] gpioPin - номер пина
void CommonFunctions_GpioToggleState(uint8_t gpioPin)
{
    // Чтение состояния пина
    PinState pinState = CommonFunctions_GpioGetState(gpioPin);

    // Если на выходе лог. "0"
    if (PIN_RESET == pinState)
    {
        // Установка на выходе лог. "1"
        CommonFunctions_GpioSetState(gpioPin, PIN_SET);
    }
    else
    {
        // Установка на выходе лог. "0"
        CommonFunctions_GpioSetState(gpioPin, PIN_RESET);
    }
}