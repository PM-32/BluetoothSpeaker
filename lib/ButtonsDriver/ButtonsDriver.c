#include <stdint.h>

#include "esp32-hal-gpio.h"
#include "soc/gpio_reg.h"

#include "ButtonsDriver.h"

#define GPIO_IN_REG_SIZE_IN_BITS        32          //!< Размер регистра GPIO_IN_REG в битах
#define ANTIBOUNCE_FILTER_IN_TICKS      200         //!< Время работы фильтра антидребезга кнопки в количестве периодов таймера 0 (20 мс)

#define BUTTON_SOUND_CONTROL_BIT_MASK       (1 << (BUTTON_SOUND_CONTROL_PIN - GPIO_IN_REG_SIZE_IN_BITS))    //!< Битовая маска для выделения пина кнопки управления звуком
#define BUTTON_INIT_BLUETOOTH_PIN_BIT_MASK  (1 << (BUTTON_INIT_BLUETOOTH_PIN - GPIO_IN_REG_SIZE_IN_BITS))   //!< Битовая маска для выделения пина кнопки инициализации Bluetooth

static uint32_t buttonsMasksArray[BUTTONS_QUANTITY] = { BUTTON_SOUND_CONTROL_BIT_MASK, \
                                                        BUTTON_INIT_BLUETOOTH_PIN_BIT_MASK };               //!< Массив битовых масок кнопок                 

static volatile ButtonState buttonsState[BUTTONS_QUANTITY];  //!< Массив состояний кнопок

// Пояснение. Устройство пинов в ESP32.
// Состояния пинов ESP32 содержатся в двух регистрах: GPIO_IN_REG и
// GPIO_IN1_REG. GPIO_IN_REG содержит состояния пинов GPIO_0 - GPIO_31
// (32 бит - [31:0]). GPIO_IN1_REG содержит состояния пинов
// GPIO_32 - GPIO_39 (8 бит - [7:0]).

//! \brief Фильтр антидребезга кнопок
void ButtonsDriver_AntibounceFilter(void)
{
    // Счетчик измерений, когда кнопка была нажата
    static uint16_t counterButtonPressed = 0;

    // Счетчик измерений, когда кнопка была не нажата
    static uint16_t counterButtonNotPressed = 0;

    // Чтение текущего состояния кнопки
    uint8_t currentButtonState = (uint8_t) (REG_READ(GPIO_IN1_REG) & buttonsMasksArray[BUTTON_SOUND_CONTROL]);

    // Если кнопка нажата
    if (LOW == currentButtonState)
    {
        // Инкремент счетчика нажатий кнопки
        counterButtonPressed++;

        // Сброс счетчика отпусканий кнопки
        counterButtonNotPressed = 0;
        
        // Если достигнуто время завершения работы фильтра антидребезга кнопки
        if (ANTIBOUNCE_FILTER_IN_TICKS <= counterButtonPressed)
        {
            // Установка статуса - зафиксировано устойчивое нажатие кнопки
            buttonsState[BUTTON_SOUND_CONTROL] = BUTTON_PRESSED_ONE_TIME;
        }
    }
    else if (HIGH == currentButtonState)    // Если кнопка отпущена
    {
        // Инкремент счетчика отпусканий кнопки
        counterButtonNotPressed++;

        // Сброс счетчика нажатий кнопки
        counterButtonPressed = 0;

        // Если достигнуто время завершения работы фильтра антидребезга кнопки
        if (ANTIBOUNCE_FILTER_IN_TICKS <= counterButtonNotPressed)
        {
            // Установка статуса - зафиксировано устойчивое отпускание кнопки
            buttonsState[BUTTON_SOUND_CONTROL] = BUTTON_NOT_PRESSED;
        }
    }
}

//! \brief Получение адреса массива состояний кнопок
ButtonState * GetCurrentButtonsState(void)
{
    return (ButtonState *) &buttonsState[0];
}