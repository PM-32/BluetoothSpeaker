#ifndef INC_BATTERY_CHARGE_INDICATION_H_
#define INC_BATTERY_CHARGE_INDICATION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Инициализация светодиода индикации заряда батареи
void BatteryChargeIndication_Init(void);

//! \brief Индикация заряда батареи
void BatteryChargeIndication_Execute(void);

#ifdef __cplusplus
}
#endif

#endif // INC_BATTERY_CHARGE_INDICATION_H_