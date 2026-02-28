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

//! \brief Фильтр антидребезга кнопок
void ButtonsDriver_AntibounceFilter(void);

//! \brief Получение адреса массива с количеством устойчивых нажатий на кнопки
//! \return Адрес массива с количеством устойчивых нажатий на кнопки
uint8_t * ButtonsDriver_GetButtonsPressCountPointer(void);

#ifdef __cplusplus
}
#endif

#endif // INC_BUTTONS_DRIVER_H_