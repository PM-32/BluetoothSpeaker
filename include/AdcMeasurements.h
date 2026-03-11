#ifndef INC_ADC_MEASUREMENTS_H_
#define INC_ADC_MEASUREMENTS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_POLLINGS_PERIOD     500     //!< Период опроса каналов АЦП в количестве периодов таймера 0 (100 мкс * 500 = 50 мс)

//! \brief Номера потенциометров
typedef enum
{
    POTENTIOMETER_VOLUME_CONTROL = 0,   //!< Потенциометр управления громкостью звука
    POTENTIOMETER_BRIGHT_CONTROL,       //!< Потенциометр управления яркостью светодиодной матрицы
    POTENTIOMETERS_QUANTITY             //!< Общее количество потенциометров
} Potentiometers;

//! \brief Инициализация АЦП
void AdcMeasurements_Init(void);

//! \brief Фильтр скользящего среднего для каналов АЦП
void AdcMeasurements_MovingAverageFilter(void);

//! \brief Получение адреса массива с отсчетами АЦП в процентах
//! \return Адрес массива с отсчетами АЦП в процентах
uint8_t * AdcMeasurements_GetAdcCountsInPercentsPointer(void);

#ifdef __cplusplus
}
#endif

#endif // INC_ADC_MEASUREMENTS_H_