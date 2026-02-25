#include <stdint.h>

#include "esp32-hal-gpio.h"
#include "soc/gpio_reg.h"

#include "ButtonsDriver.h"
#include "UserTimer.h"

#define GPIO_IN_REG_SIZE_IN_BITS        32          //!< Размер регистра GPIO_IN_REG в битах
#define ANTIBOUNCE_FILTER_IN_TICKS      200         //!< Время работы фильтра антидребезга кнопки в количестве периодов таймера 0 (20 мс)
#define NEXT_BUTTON_PRESS_TIMEOUT       5000        //!< Таймаут ожидания следующего нажатия на кнопку в количестве периодов таймера 0 (500 мс)

#define BUTTON_SOUND_CONTROL_BIT_MASK       (1 << (BUTTON_SOUND_CONTROL_PIN - GPIO_IN_REG_SIZE_IN_BITS))    //!< Битовая маска для выделения пина кнопки управления звуком
#define BUTTON_INIT_BLUETOOTH_PIN_BIT_MASK  (1 << (BUTTON_INIT_BLUETOOTH_PIN - GPIO_IN_REG_SIZE_IN_BITS))   //!< Битовая маска для выделения пина кнопки инициализации Bluetooth

static volatile ButtonState buttonsState[BUTTONS_QUANTITY];                                                 //!< Массив состояний кнопок

static uint32_t buttonsMasksArray[BUTTONS_QUANTITY] = { BUTTON_SOUND_CONTROL_BIT_MASK, \
                                                        BUTTON_INIT_BLUETOOTH_PIN_BIT_MASK };               //!< Массив битовых масок кнопок                 

static uint8_t counterButtonsPress[BUTTONS_QUANTITY];                                                       //!< Массив с счетчиками зафиксированных подряд нажатий на кнопки

// Пояснение. Устройство пинов в ESP32.
// Состояния пинов ESP32 содержатся в двух регистрах: GPIO_IN_REG и
// GPIO_IN1_REG. GPIO_IN_REG содержит состояния пинов GPIO_0 - GPIO_31
// (32 бит - [31:0]). GPIO_IN1_REG содержит состояния пинов
// GPIO_32 - GPIO_39 (8 бит - [7:0]).

//! \brief Фильтр антидребезга кнопок
void ButtonsDriver_AntibounceFilter(void)
{
    // Счетчик для подсчета временных интервалов 
    // по 100 мкс, когда кнопка была нажата
    static uint16_t counterButtonPressed = 0;

    // Счетчик для подсчета временных интервалов 
    // по 100 мкс, когда кнопка не была нажата
    static uint16_t counterButtonNotPressed = 0;

    // Статус работы таймера для отсчета таймаута следующего нажатия на кнопку
    static TimerWaitNextButtonPressStatus timerWaitSecondButtonPressStatus = TIMER_WAIT_NEXT_BUTTON_PRESS_OFF;

    // Время начала ожидания следующего нажатия на кнопку
    static uint32_t waitNextButtonPressStartTime = 0;

    // Чтение текущего состояния кнопки
    uint8_t currentButtonState = (uint8_t) (REG_READ(GPIO_IN1_REG) & buttonsMasksArray[BUTTON_SOUND_CONTROL]);

    // Если включен таймер ожидания второго нажатия на кнопку
    if (TIMER_WAIT_NEXT_BUTTON_PRESS_ON == timerWaitSecondButtonPressStatus)
    {
        // Если не вышел таймаут ожидания второго нажатия на кнопку
        if (NEXT_BUTTON_PRESS_TIMEOUT > (UserTimer_GetCounterTime() - waitNextButtonPressStartTime))
        {
            // Если кнопка нажата
            if (LOW == currentButtonState)
            {
                // Увеличение счетчика нажатий кнопки
                counterButtonPressed++;

                // Сброс счетчика отжатий кнопки
                counterButtonNotPressed = 0;

                // Если в течение работы фильтра антидребезга кнопка была нажата
                if (ANTIBOUNCE_FILTER_IN_TICKS <= counterButtonPressed)
                {
                    // Установка статуса - зафиксировано два подряд устойчивых нажатия на кнопку
                    buttonsState[BUTTON_SOUND_CONTROL] = BUTTON_PRESSED_TWO_TIMES;
                }
            }
            else // Если кнопка отжата
            {
                // Увеличение счетчика отжатий кнопки
                counterButtonNotPressed++;

                // Сброс счетчика нажатий кнопки
                counterButtonPressed = 0;
            }
        }
        else // Если вышел таймаут ожидания второго нажатия на кнопку
        {
            // Выключение таймера для отсчета таймаута второго нажатия на кнопку
            timerWaitSecondButtonPressStatus = TIMER_WAIT_NEXT_BUTTON_PRESS_OFF;

            // Сброс времени начала ожидания следующего нажатия на кнопку
            waitNextButtonPressStartTime = 0;
        }
    }
    else // Если таймер ожидания второго нажатия на кнопку выключен
    {
        // Если кнопка нажата
        if (LOW == currentButtonState)
        {
            // Увеличение счетчика нажатий кнопки
            counterButtonPressed++;

            // Сброс счетчика отжатий кнопки
            counterButtonNotPressed = 0;

            // Если в течение работы фильтра антидребезга кнопка была нажата
            if (ANTIBOUNCE_FILTER_IN_TICKS <= counterButtonPressed)
            {
                // Если не было зафиксировано второе устойчивое нажатие на кнопку
                if (BUTTON_PRESSED_TWO_TIMES != buttonsState[BUTTON_SOUND_CONTROL])
                {
                    // Установка статуса - зафиксировано устойчивое нажатие кнопки
                    buttonsState[BUTTON_SOUND_CONTROL] = BUTTON_PRESSED_ONE_TIME;
                }
            }
        }
        else // Если кнопка отжата
        {
            // Увеличение счетчика отжатий кнопки
            counterButtonNotPressed++;

            // Сброс счетчика нажатий кнопки
            counterButtonPressed = 0;

            // Если в течение работы фильтра антидребезга кнопка была отжата
            if (ANTIBOUNCE_FILTER_IN_TICKS <= counterButtonNotPressed)
            {
                // Если ранее было зафиксировано
                // первое устойчивое нажатие кнопки
                if (BUTTON_PRESSED_ONE_TIME == buttonsState[BUTTON_SOUND_CONTROL])
                {
                    // Включение таймера ожидания второго нажатия на кнопку
                    timerWaitSecondButtonPressStatus = TIMER_WAIT_NEXT_BUTTON_PRESS_ON;

                    // Сохранение времени начала ожидания следующего нажатия на кнопку
                    waitNextButtonPressStartTime = UserTimer_GetCounterTime();
                }

                // Установка статуса - зафиксировано устойчивое отпускание кнопки
                buttonsState[BUTTON_SOUND_CONTROL] = BUTTON_NOT_PRESSED;
            }
        }
    }
}

//! \brief Получение адреса массива состояний кнопок
ButtonState * GetCurrentButtonsState(void)
{
    return (ButtonState *) &buttonsState[0];
}