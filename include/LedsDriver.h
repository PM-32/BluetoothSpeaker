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

//! \brief Моргание светодиодом с заданным периодом
//! \param[in] ledPin - пин светодиода
//! \param[in] period - период включения светодиода
//!                     в количестве периодов таймера 0
void LedsDriver_BlinkLed(uint8_t ledPin, uint32_t period);

#ifdef __cplusplus
}
#endif

#endif // LEDS_DRIVER_H_