#ifndef INC_BATTERY_CHARGE_INDICATION_H_
#define INC_BATTERY_CHARGE_INDICATION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BATTERY_CHARGE_LED_PIN          2       //!< Светодиод для индикации заряда батареи (WS2812)

//! \brief Предустановленные цвета индикации
typedef enum
{
    BATTERY_COLOR_RED = 0,      //!< Красный (заряд < 30%)
    BATTERY_COLOR_YELLOW,       //!< Жёлтый (заряд 30% - 70%)
    BATTERY_COLOR_GREEN,        //!< Зелёный (заряд > 70%)
    BATTERY_COLOR_OFF           //!< Выключен
} BatteryChargeColor;

//! \brief Инициализация индикации заряда батареи (WS2812)
//! \param[in] pin - номер пина GPIO для подключения светодиода
void BatteryChargeIndication_Init(uint8_t pin);

//! \brief Установка цвета светодиода по компонентам RGB
//! \param[in] red - компонент красного (0-255)
//! \param[in] green - компонент зелёного (0-255)
//! \param[in] blue - компонент синего (0-255)
void BatteryChargeIndication_SetRgbColor(uint8_t red, uint8_t green, uint8_t blue);

//! \brief Установка цвета индикации по предустановленному значению
//! \param[in] color - цвет из перечисления BatteryChargeColor
void BatteryChargeIndication_SetColor(BatteryChargeColor color);

//! \brief Периодическое обновление состояния индикации
//! \note Должна вызываться в основном цикле программы
void BatteryChargeIndication_Update(void);

#ifdef __cplusplus
}
#endif

#endif // INC_BATTERY_CHARGE_INDICATION_H_