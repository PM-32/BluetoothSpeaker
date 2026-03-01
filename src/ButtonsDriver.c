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

// Пояснение. Устройство пинов в ESP32.
// Состояния пинов ESP32 содержатся в двух регистрах: GPIO_IN_REG и
// GPIO_IN1_REG. GPIO_IN_REG содержит состояния пинов GPIO_0 - GPIO_31
// (32 бит - [31:0]). GPIO_IN1_REG содержит состояния пинов
// GPIO_32 - GPIO_39 (8 бит - [7:0]).

//! \brief Фильтр антидребезга кнопок
void ButtonsDriver_AntibounceFilter(void)
{
    // Массив битовых масок кнопок
    static uint32_t buttonsMasksArray[BUTTONS_QUANTITY] = { BUTTON_SOUND_CONTROL_BIT_MASK, \
                                                            BUTTON_INIT_BLUETOOTH_PIN_BIT_MASK };

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
        uint8_t currentButtonState = (uint8_t) (REG_READ(GPIO_IN1_REG) & buttonsMasksArray[buttonIndex]);

        // Если запущен таймер ожидания следующего нажатия на кнопку
        if (WAITING_FOR_NEXT_PRESS == severalPressFilterStage[buttonIndex])
        {
            // Если достигнут таймаут ожидания следующего нажатия на кнопку
            if (NEXT_BUTTON_PRESS_TIMEOUT <= UserTimer_GetCounterTime() - waitingForNextPressStartTime[buttonIndex])
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
        if (LOW == currentButtonState)
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
                    // Увеличение счетчика нажатий
                    buttonsPressCount[buttonIndex] = waitingStartPress[buttonIndex] + 1;

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

//! \brief Получение адреса массива со статусами завершения серий нажатий
//! \return Адрес массива со статусами завершения серий нажатий
ButtonPressSeriesStatus * ButtonsDriver_GetButtonsPressSeriesStatusPointer(void)
{
    return (ButtonPressSeriesStatus *) buttonsPressSeriesStatus;
}