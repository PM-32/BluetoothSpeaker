#ifndef INC_ADC_MEASUREMENTS_H_
#define INC_ADC_MEASUREMENTS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Инициализация АЦП
void AdcMeasurements_Init(void);

//! \brief Периодический опрос канала АЦП
void AdcMeasurements_Pollings(void);

//! \brief Получение отсчетов АЦП потенциометра в процентах
//! \return Отсчеты АЦП потенциометра в процентах
uint8_t AdcMeasurements_GetPotentiometerAdcCountsInPercents(void);

#ifdef __cplusplus
}
#endif

#endif // INC_ADC_MEASUREMENTS_H_