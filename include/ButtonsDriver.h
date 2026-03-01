#ifndef INC_BUTTONS_DRIVER_H_
#define INC_BUTTONS_DRIVER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_SOUND_CONTROL_PIN        32      //!< Номер пина кнопки управления звуком
#define BUTTON_INIT_BLUETOOTH_PIN       33      //!< Номер пина кнопки инициализации Bluetooth

//! \brief Номера кнопок
typedef enum
{
    BUTTON_SOUND_CONTROL = 0,                   //!< Кнопка управления звуком
    BUTTON_INIT_BLUETOOTH,                      //!< Кнопка инициализации Bluetooth
    BUTTONS_QUANTITY                            //!< Общее количество кнопок
} Buttons;

//! \brief Количество нажатий на кнопку
typedef enum
{
    ONE_PRESS = 1,                              //!< Одно нажатие на кнопку
    TWO_PRESS,                                  //!< Два нажатия на кнопку
    THREE_PRESS                                 //!< Три нажатия на кнопку
} ButtonPresses;

//! \brief Статус завершения серии нажатий на кнопку
typedef enum
{
    BUTTON_PRESS_SERIES_NONE = 0,               //!< Серия нажатий на кнопку не завершена
    BUTTON_PRESS_SERIES_FINISHED                //!< Серия нажатий на кнопку завершена по таймауту
} ButtonPressSeriesStatus;

//! \brief Фильтр антидребезга кнопок
void ButtonsDriver_AntibounceFilter(void);

//! \brief Получение адреса массива с количеством устойчивых нажатий на кнопки
//! \return Адрес массива с количеством устойчивых нажатий на кнопки
uint8_t * ButtonsDriver_GetButtonsPressCountPointer(void);

//! \brief Получение адреса массива со статусами завершения серий нажатий на кнопки
//! \return Адрес массива со статусами завершения серий нажатий на кнопки
ButtonPressSeriesStatus * ButtonsDriver_GetButtonsPressSeriesStatusPointer(void);

#ifdef __cplusplus
}
#endif

#endif // INC_BUTTONS_DRIVER_H_