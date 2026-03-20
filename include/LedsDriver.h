#ifndef LEDS_DRIVER_H_
#define LEDS_DRIVER_H_

#include <stdint.h>

#define BLUETOOTH_STATUS_LED_PIN    4       //!< Пин, к которому подключен светодиод индикации состояния Bluetooth-подключения

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Состояние светодиода
typedef enum
{
    LED_OFF = 0,    //!< Светодиод выключен
    LED_ON          //!< Светодиод включен
} LedState;

//! \brief Инициализация светодиода
void LedsDriver_Init(void);

//! \brief Установка состояния светодиода
//! \param[in] led - номер светодиода
//! \param[in] ledState - состояние светодиода
void LedsDriver_SetLedState(uint8_t ledPin, LedState ledState);

//! \brief Смена состояния светодиода
//! \param[in] ledPin - пин светодиода
void LedsDriver_ToggleLed(uint8_t ledPin);

#ifdef __cplusplus
}
#endif

#endif // LEDS_DRIVER_H_