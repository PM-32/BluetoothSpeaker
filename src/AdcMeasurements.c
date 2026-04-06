#include "esp32-hal-adc.h"

#include "AdcMeasurements.h"
#include "UserTimer.h"

#define POTENTIOMETER_VOLUME_CONTROL_PIN        34          //!< Номер пина потенциометра управления громкостью звука

#define ADC_POLLINGS_PERIOD                     500         //!< Период опроса каналов АЦП в количестве периодов таймера 0 (100 мкс * 500 = 50 мс)
#define ADC_MEASUREMENTS_QUANTITY               5           //!< Количество измерений АЦП для заполнения буфера измерений
#define ADC_RESOLUTION                          12          //!< Разрядность АЦП
#define ADC_MAX_COUNTS                          4095        //!< Максимальное значение отсчётов АЦП
#define MAX_PERCENTS                            100         //!< Максимальное значение отсчётов АЦП в процентах
#define ADC_COUNTS_TO_PERCENTS_COEFF            ((float) MAX_PERCENTS / ADC_MAX_COUNTS) //!< Коэффициент для перевода отсчетов АЦП в проценты

#define POTENTIOMETER_VOLUME_CONTROL_C0         1.0252f     //!< Калибровочный коэффициент C0 для потенциометра управления громкостью звука
#define POTENTIOMETER_VOLUME_CONTROL_C1         11.5537f    //!< Калибровочный коэффициент C1 для потенциометра управления громкостью звука

// Пояснение. В целях упрощения было принято решение один раз
// откалибровать потенциометр с помощью метода наименьших квадратов,
// не реализовывая калибровку программно. Двухпараметрическая
// калибровка - оптимальный вариант: корректируется и наклон, и смещение.

static uint8_t potentiometerAdcCountsInPercents;    //!< Отсчеты АЦП потенциометра в процентах

//! \brief Инициализация АЦП
void AdcMeasurements_Init(void)
{
    // Установка разрядности АЦП для всех пинов
    analogReadResolution(ADC_RESOLUTION);
    
    // Установка диапазона измерений 0 - 3.3V для пина потенциометра
    analogSetPinAttenuation(POTENTIOMETER_VOLUME_CONTROL_PIN, ADC_11db);
}

//! \brief Фильтр скользящего среднего для канала АЦП
static void AdcMeasurements_MovingAverageFilter(void)
{
    // Массив измерений значений АЦП потенциометра
    static uint16_t adcValues[ADC_MEASUREMENTS_QUANTITY];
    
    // Суммарное значение АЦП канала
    static uint32_t sumAdcValue = 0;
    
    // Индекс текущего измерения АЦП
    static uint8_t measurementIndex = 0;
    
    // Счетчик заполненных измерений в буфере АЦП
    // (счетчик заходов в функцию)
    static uint8_t measurementsCount = 0;
    
    // Получение текущего значения АЦП
    uint16_t currentAdcValue = (uint16_t) analogRead(POTENTIOMETER_VOLUME_CONTROL_PIN);
    
    // Вычитание старого значения из суммарного
    // значения АЦП, если буфер уже заполнен
    if (measurementsCount >= ADC_MEASUREMENTS_QUANTITY)
    {
        sumAdcValue -= adcValues[measurementIndex];
    }
    
    // Сохранение нового значения АЦП
    adcValues[measurementIndex] = currentAdcValue;
    
    // Добавление нового значения в суммарное значение АЦП
    sumAdcValue += currentAdcValue;
    
    // Увеличение индекса измерений АЦП
    measurementIndex++;
    
    // Сброс индекса измерений АЦП
    if (measurementIndex >= ADC_MEASUREMENTS_QUANTITY)
    {
        measurementIndex = 0;
    }
    
    // Увеличение счетчика заполненных измерений
    if (measurementsCount < ADC_MEASUREMENTS_QUANTITY)
    {
        measurementsCount++;
    }
    
    // Если выполнено, как минимум, одно измерение АЦП
    if (measurementsCount > 0)
    {
        // Вычисление усредненного значения отсчетов АЦП
        uint16_t meanAdcValue = (uint16_t) (sumAdcValue / measurementsCount);
        
        // Вычисление усредненного значения отсчетов АЦП с калибровкой
        uint16_t meanCalibratedAdcCounts = (uint16_t) (POTENTIOMETER_VOLUME_CONTROL_C0 * meanAdcValue + POTENTIOMETER_VOLUME_CONTROL_C1);
        
        // Ограничение отсчетов АЦП
        if (meanCalibratedAdcCounts > ADC_MAX_COUNTS)
        {
            meanCalibratedAdcCounts = ADC_MAX_COUNTS;
        }
        
        // Вычисление отсчетов АЦП в процентах
        potentiometerAdcCountsInPercents = (uint8_t) (meanCalibratedAdcCounts * ADC_COUNTS_TO_PERCENTS_COEFF);
        
        // Ограничение процентов
        if (potentiometerAdcCountsInPercents > MAX_PERCENTS)
        {
            potentiometerAdcCountsInPercents = MAX_PERCENTS;
        }
    }
}

//! \brief Периодический опрос канала АЦП
void AdcMeasurements_Pollings(void)
{
    // Время последнего опроса АЦП
    static uint32_t lastAdcPollingTime = 0;
    
    // Чтение текущего положения ручки потенциометра
    // каждые ADC_POLLINGS_PERIOD периодов таймера 0
    if ((UserTimer_GetCounterTime() - lastAdcPollingTime) > ADC_POLLINGS_PERIOD)
    {
        // Фильтр скользящего среднего для канала АЦП
        AdcMeasurements_MovingAverageFilter();
        
        // Обновление времени последнего опроса АЦП
        lastAdcPollingTime = UserTimer_GetCounterTime();
    }
}

//! \brief Получение отсчетов АЦП потенциометра в процентах
//! \return Отсчеты АЦП потенциометра в процентах
uint8_t AdcMeasurements_GetPotentiometerAdcCountsInPercents(void)
{
    return potentiometerAdcCountsInPercents;
}