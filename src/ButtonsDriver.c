#include <stdint.h>

#include "esp32-hal-gpio.h"
#include "soc/gpio_reg.h"

#include "ButtonsDriver.h"
#include "CommonFunctions.h"
#include "UserTimer.h"

#define ANTIBOUNCE_FILTER_IN_TICKS      200         //!< Время работы фильтра антидребезга кнопки в количестве периодов таймера 0 (20 мс)
#define NEXT_BUTTON_PRESS_TIMEOUT       5000        //!< Таймаут ожидания следующего нажатия на кнопку в количестве периодов таймера 0 (500 мс)

//! \brief Стадии работы фильтра антидребезга
//!        кнопки для определения многократных нажатий
typedef enum
{
    START_SEVERAL_PRESS_FILTER = 0,     //!< Начальная стадия (состояние сброса)
    FIRST_PRESS_DETECTED,               //!< Зафиксировано первое устойчивое нажатие на кнопку
    WAITING_FOR_NEXT_PRESS              //!< Ожидание следующего нажатия на кнопку
} SeveralPressFilterStage;

//! \brief Статус определения нового нажатия на кнопку
typedef enum
{
    NEW_PRESS_NOT_DETECTED = 0,         //!< Новое нажатие не зафиксировано
    NEW_PRESS_DETECTED                  //!< Новое нажатие зафиксировано
} NewPressDetectedStatus;
          
static volatile uint8_t buttonsPressCount[BUTTONS_QUANTITY];                        //!< Массив с количеством устойчивых нажатий на кнопки
static volatile ButtonPressSeriesStatus buttonsPressSeriesStatus[BUTTONS_QUANTITY]; //!< Массив со статусами завершения серий нажатий

//! \brief Инициализация кнопок
void ButtonsDriver_Init(void)
{
    // Настройка пина кнопки управления звуком
    // (подтяжка к +3.3V)
    pinMode(BUTTON_SOUND_CONTROL_PIN, INPUT_PULLUP);

    // Настройка пина кнопки для инициализации Bluetooth
    // (подтяжка к +3.3V)
    pinMode(BUTTON_BLUETOOTH_CONNECTION_CONTROL_PIN, INPUT_PULLUP);
}

//! \brief Фильтр антидребезга кнопок
void ButtonsDriver_AntibounceFilter(void)
{
    // Массив пинов кнопок
    static uint8_t buttonsPinArray[BUTTONS_QUANTITY] = { BUTTON_SOUND_CONTROL_PIN, \
                                                         BUTTON_BLUETOOTH_CONNECTION_CONTROL_PIN };

    // Массив с предельными количествами фиксируемых нажатий на кнопки
    static uint8_t maxButtonsPress[BUTTONS_QUANTITY] = { THREE_PRESS, TWO_PRESS };

    // Текущая стадия работы фильтра антидребезга
    // кнопки для определения многократных нажатий
    static volatile SeveralPressFilterStage severalPressFilterStage[BUTTONS_QUANTITY];

    // Статус определения нового нажатия на кнопку
    static volatile NewPressDetectedStatus newPressDetectedStatus[BUTTONS_QUANTITY];

    // Время, когда был запущен таймер для отсчета
    // таймаута следующего нажатия на кнопку
    static volatile uint32_t waitingForNextPressStartTime[BUTTONS_QUANTITY];

    // Счетчик времени, когда кнопка была нажата
    static volatile uint16_t counterButtonPressed[BUTTONS_QUANTITY];

    // Счетчик времени, когда кнопка была не нажата
    static volatile uint16_t counterButtonNotPressed[BUTTONS_QUANTITY];

    // Количество зафиксированных устойчивых нажатий на кнопку в момент
    // запуска таймера для отсчета таймаута следующего нажатия на кнопку
    static volatile uint8_t waitingStartPress[BUTTONS_QUANTITY];

    for (uint8_t buttonIndex = 0; buttonIndex < BUTTONS_QUANTITY; buttonIndex++)
    {
        // Чтение текущего состояния кнопки
        PinState currentButtonState = CommonFunctions_GpioGetState(buttonsPinArray[buttonIndex]);

        // Если запущен таймер ожидания следующего нажатия на кнопку
        if (WAITING_FOR_NEXT_PRESS == severalPressFilterStage[buttonIndex])
        {
            // Если достигнут таймаут ожидания следующего нажатия на кнопку
            if (NEXT_BUTTON_PRESS_TIMEOUT <= (UserTimer_GetCounterTime() - waitingForNextPressStartTime[buttonIndex]))
            {
                // Если было хотя бы одно нажатие
                if (buttonsPressCount[buttonIndex] > 0)
                {
                    // Установка статуса - серия нажатий на кнопку завершена
                    buttonsPressSeriesStatus[buttonIndex] = BUTTON_PRESS_SERIES_FINISHED;
                }
                
                // Сброс времени, когда был запущен таймер
                // для отсчета таймаута следующего нажатия на кнопку
                waitingForNextPressStartTime[buttonIndex] = 0;

                // Сброс стадии работы фильтра антидребезга
                // кнопки для определения многократных нажатий
                severalPressFilterStage[buttonIndex] = START_SEVERAL_PRESS_FILTER;
            }
        }

        // Если кнопка нажата
        if (PIN_RESET == currentButtonState)
        {
            // Увеличение счетчика времени,
            // когда кнопка была нажата
            counterButtonPressed[buttonIndex]++;

            // Сброс счетчика времени,
            // когда кнопка была не нажата
            counterButtonNotPressed[buttonIndex] = 0;

            // Если кнопка была нажата в течение
            // времени работы фильтра антидребезга
            if (ANTIBOUNCE_FILTER_IN_TICKS <= counterButtonPressed[buttonIndex])
            {
                // Если запущен таймер ожидания следующего нажатия на кнопку
                if (WAITING_FOR_NEXT_PRESS == severalPressFilterStage[buttonIndex])
                {
                    // Ограничение максимального количества фиксируемых нажатий на кнопку
                    if (buttonsPressCount[buttonIndex] < maxButtonsPress[buttonIndex])
                    {
                        // Увеличение счетчика нажатий
                        buttonsPressCount[buttonIndex] = waitingStartPress[buttonIndex] + 1;
                    }

                    // Установка статуса - новое нажатие на кнопку зафиксировано
                    newPressDetectedStatus[buttonIndex] = NEW_PRESS_DETECTED;
                    
                    // Сброс статуса завершения серии нажатий на кнопку
                    buttonsPressSeriesStatus[buttonIndex] = BUTTON_PRESS_SERIES_NONE;
                }
                else // Если не запущен таймер ожидания следующего нажатия на кнопку
                {
                    // Если не было зафиксировано новое нажатие на кнопку
                    if (NEW_PRESS_DETECTED != newPressDetectedStatus[buttonIndex])
                    {
                        // Зафиксировано первое устойчивое нажатие на кнопку
                        buttonsPressCount[buttonIndex] = 1;
                        
                        // Сброс статуса завершения серии нажатий на кнопку
                        buttonsPressSeriesStatus[buttonIndex] = BUTTON_PRESS_SERIES_NONE;
                    }

                    // Если фильтр антидребезга кнопки для определения
                    // многократного нажатия находится в стадии сброса
                    if (START_SEVERAL_PRESS_FILTER == severalPressFilterStage[buttonIndex])
                    {
                        // Установка стадии - зафиксировано первое устойчивое нажатие на кнопку
                        severalPressFilterStage[buttonIndex] = FIRST_PRESS_DETECTED;
                    }
                }
            }
        }
        else // Если кнопка не нажата
        {
            // Увеличение счетчика времени,
            // когда кнопка была не нажата
            counterButtonNotPressed[buttonIndex]++;

            // Сброс счетчика времени,
            // когда кнопка была нажата
            counterButtonPressed[buttonIndex] = 0;

            // Если кнопка была не нажата в течение
            // времени работы фильтра антидребезга
            if (ANTIBOUNCE_FILTER_IN_TICKS <= counterButtonNotPressed[buttonIndex])
            {
                // Если зафиксировано первое устойчивое нажатие на
                // кнопку или зафиксировано новое нажатие на кнопку
                if ((FIRST_PRESS_DETECTED == severalPressFilterStage[buttonIndex]) ||
                    (NEW_PRESS_DETECTED == newPressDetectedStatus[buttonIndex]))
                {
                    // Установка стадии - ожидание следующего нажатия на кнопку
                    // (запуск таймера для отсчета таймаута следующего нажатия на кнопку)
                    severalPressFilterStage[buttonIndex] = WAITING_FOR_NEXT_PRESS;

                    // Сохранение времени, когда был запущен таймер
                    // для отсчета таймаута следующего нажатия на кнопку
                    waitingForNextPressStartTime[buttonIndex] = UserTimer_GetCounterTime();

                    // Сохранение количества нажатий на кнопку в момент запуска
                    // таймера для отсчета таймаута следующего нажатия на кнопку
                    waitingStartPress[buttonIndex] = buttonsPressCount[buttonIndex];

                    // Сброс статуса определения нового нажатия на кнопку
                    newPressDetectedStatus[buttonIndex] = NEW_PRESS_NOT_DETECTED;
                }
            }
        }
    }
}

//! \brief Получение адреса массива с количеством устойчивых нажатий на кнопки
//! \return Адрес массива с количеством устойчивых нажатий на кнопки
uint8_t * ButtonsDriver_GetButtonsPressCountPointer(void)
{
    return (uint8_t *) buttonsPressCount;
}

//! \brief Получение адреса массива со статусами завершения серий нажатий на кнопки
//! \return Адрес массива со статусами завершения серий нажатий на кнопки
ButtonPressSeriesStatus * ButtonsDriver_GetButtonsPressSeriesStatusPointer(void)
{
    return (ButtonPressSeriesStatus *) buttonsPressSeriesStatus;
}