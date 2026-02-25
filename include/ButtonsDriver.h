#ifndef INC_BUTTONS_DRIVER_H_
#define INC_BUTTONS_DRIVER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_SOUND_CONTROL_PIN        32      //!< Номер пина кнопки управления звуком
#define BUTTON_INIT_BLUETOOTH_PIN       33      //!< Номер пина кнопки инициализации Bluetooth

//! \brief Возможные состояния кнопки
typedef enum
{
    BUTTON_NOT_PRESSED = 0,         //!< Кнопка не нажата
    BUTTON_PRESSED_ONE_TIME,        //!< Зафиксировано одно устойчивое нажатие кнопки
    BUTTON_PRESSED_TWO_TIMES,       //!< Зафиксировано два подряд устойчивых нажатия на кнопку
    BUTTON_PRESSED_THREE_TIMES      //!< Зафиксировано три подряд устойчивых нажатия на кнопку
} ButtonState;

//! \brief Номера кнопок
typedef enum
{
    BUTTON_SOUND_CONTROL = 0,       //!< Кнопка управления звуком
    BUTTON_INIT_BLUETOOTH,          //!< Кнопка инициализации Bluetooth
    BUTTONS_QUANTITY                //!< Общее количество кнопок
} Buttons;

//! \brief Статус работы таймера для отсчета таймаута следующего нажатия на кнопку
typedef enum
{
    TIMER_WAIT_NEXT_BUTTON_PRESS_OFF = 0,
    TIMER_WAIT_NEXT_BUTTON_PRESS_ON
} TimerWaitNextButtonPressStatus;

//! \brief Фильтр антидребезга кнопок
void ButtonsDriver_AntibounceFilter(void);

//! \brief Получение адреса массива состояний кнопок
ButtonState * GetCurrentButtonsState(void);

#ifdef __cplusplus
}
#endif

#endif // INC_BUTTONS_DRIVER_H_