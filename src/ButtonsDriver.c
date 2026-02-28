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

static SeveralPressFilterStage severalPressFilterStage = START_SEVERAL_PRESS_FILTER;            //!< Текущая стадия работы фильтра антидребезга
                                                                                                //!< кнопки для определения многократных нажатий
static NewPressDetectedStatus newPressDetectedStatus = NEW_PRESS_NOT_DETECTED;                  //!< Статус определения нового нажатия на кнопку

static uint32_t buttonsMasksArray[BUTTONS_QUANTITY] = { BUTTON_SOUND_CONTROL_BIT_MASK, \
                                                        BUTTON_INIT_BLUETOOTH_PIN_BIT_MASK };   //!< Массив битовых масок кнопок                 

static uint8_t buttonsPressCount[BUTTONS_QUANTITY];    //!< Массив с количеством устойчивых нажатий на кнопки

// Пояснение. Устройство пинов в ESP32.
// Состояния пинов ESP32 содержатся в двух регистрах: GPIO_IN_REG и
// GPIO_IN1_REG. GPIO_IN_REG содержит состояния пинов GPIO_0 - GPIO_31
// (32 бит - [31:0]). GPIO_IN1_REG содержит состояния пинов
// GPIO_32 - GPIO_39 (8 бит - [7:0]).

//! \brief Фильтр антидребезга кнопок
void ButtonsDriver_AntibounceFilter(void)
{
    // Время, когда был запущен таймер для отсчета
    // таймаута следующего нажатия на кнопку
    static uint32_t waitingForNextPressStartTime = 0;

    // Количество зафиксированных устойчивых нажатий на кнопку в момент
    // запуска таймера для отсчета таймаута следующего нажатия на кнопку
    static uint8_t waitingStartPress = 0;

    // Счетчик времени, когда кнопка была нажата
    static uint16_t counterButtonPressed = 0;

    // Счетчик времени, когда кнопка была не нажата
    static uint16_t counterButtonNotPressed = 0;    

    // Чтение текущего состояния кнопки
    uint8_t currentButtonState = (uint8_t) (REG_READ(GPIO_IN1_REG) & buttonsMasksArray[BUTTON_SOUND_CONTROL]);

    // Если запущен таймер ожидания следующего нажатия на кнопку
    if (WAITING_FOR_NEXT_PRESS == severalPressFilterStage)
    {
        // Если достигнут таймаут ожидания следующего нажатия на кнопку
        if (NEXT_BUTTON_PRESS_TIMEOUT <= UserTimer_GetCounterTime() - waitingForNextPressStartTime)
        {
            // Сброс времени, когда был запущен таймер
            // для отсчета таймаута следующего нажатия на кнопку
            waitingForNextPressStartTime = 0;

            // Сброс стадии работы фильтра антидребезга
            // кнопки для определения многократных нажатий
            severalPressFilterStage = START_SEVERAL_PRESS_FILTER;
        }
    }

    // Если кнопка нажата
    if (LOW == currentButtonState)
    {
        // Увеличение счетчика времени,
        // когда кнопка была нажата
        counterButtonPressed++;

        // Сброс счетчика времени,
        // когда кнопка была не нажата
        counterButtonNotPressed = 0;

        // Если кнопка была нажата в течение
        // времени работы фильтра антидребезга
        if (ANTIBOUNCE_FILTER_IN_TICKS <= counterButtonPressed)
        {
            // Если запущен таймер ожидания следующего нажатия на кнопку
            if (WAITING_FOR_NEXT_PRESS == severalPressFilterStage)
            {
                // Увеличение счетчика нажатий
                buttonsPressCount[BUTTON_SOUND_CONTROL] = waitingStartPress + 1;

                // Установка статуса - новое нажатие на кнопку зафиксировано
                newPressDetectedStatus = NEW_PRESS_DETECTED;
            }
            else // Если не запущен таймер ожидания следующего нажатия на кнопку
            {
                // Если не было зафиксировано новое нажатие на кнопку
                if (NEW_PRESS_DETECTED != newPressDetectedStatus)
                {
                    // Зафиксировано первое устойчивое нажатие на кнопку
                    buttonsPressCount[BUTTON_SOUND_CONTROL] = 1;
                }

                // Если фильтр антидребезга кнопки для определения
                // многократного нажатия находится в стадии сброса
                if (START_SEVERAL_PRESS_FILTER == severalPressFilterStage)
                {
                    // Установка стадии - зафиксировано первое устойчивое нажатие на кнопку
                    severalPressFilterStage = FIRST_PRESS_DETECTED;
                }
            }
        }
    }
    else // Если кнопка не нажата
    {
        // Увеличение счетчика времени,
        // когда кнопка была не нажата
        counterButtonNotPressed++;

        // Сброс счетчика времени,
        // когда кнопка была нажата
        counterButtonPressed = 0;

        // Если кнопка была не нажата в течение
        // времени работы фильтра антидребезга
        if (ANTIBOUNCE_FILTER_IN_TICKS <= counterButtonNotPressed)
        {
            // Если зафиксировано первое устойчивое нажатие на
            // кнопку или зафиксировано новое нажатие на кнопку
            if ((FIRST_PRESS_DETECTED == severalPressFilterStage) || 
                (NEW_PRESS_DETECTED == newPressDetectedStatus))
            {
                // Установка стадии - ожидание следующего нажатия на кнопку
                // (запуск таймера для отсчета таймаута следующего нажатия на кнопку)
                severalPressFilterStage = WAITING_FOR_NEXT_PRESS;

                // Сохранение времени, когда был запущен таймер
                // для отсчета таймаута следующего нажатия на кнопку
                waitingForNextPressStartTime = UserTimer_GetCounterTime();

                // Сохранение количества нажатий на кнопку в момент запуска
                // таймера для отсчета таймаута следующего нажатия на кнопку
                waitingStartPress = buttonsPressCount[BUTTON_SOUND_CONTROL];

                // Сброс статуса определения нового нажатия на кнопку
                newPressDetectedStatus = NEW_PRESS_NOT_DETECTED;
            }

            // Текущее количество нажатий на кнопку равно 0
            buttonsPressCount[BUTTON_SOUND_CONTROL] = 0;
        }
    }
}

//! \brief Получение адреса массива с количеством устойчивых нажатий на кнопки
//! \return Адрес массива с количеством устойчивых нажатий на кнопки
uint8_t * ButtonsDriver_GetButtonsPressCountPointer(void)
{
    return buttonsPressCount;
}