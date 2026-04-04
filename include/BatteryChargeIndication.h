#ifndef INC_BATTERY_CHARGE_INDICATION_H_
#define INC_BATTERY_CHARGE_INDICATION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Инициализация светодиода индикации заряда батареи
void BatteryChargeIndication_Init(void);

//! \brief Выключение светодиода индикации заряда батареи
void BatteryChargeIndication_LedOff(void);

//! \brief Обновление цвета светодиода индикации заряда батареи
//! \note Временная проверка работы светодиода -
//!       переключение 5 цветов с заданным периодом
void BatteryChargeIndication_UpdateColor(void);

#ifdef __cplusplus
}
#endif

#endif // INC_BATTERY_CHARGE_INDICATION_H_