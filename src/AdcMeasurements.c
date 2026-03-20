#include "esp32-hal-adc.h"

#include "AdcMeasurements.h"
#include "UserTimer.h"

#define POTENTIOMETER_VOLUME_CONTROL_PIN        34          //!< Номер пина потенциометра управления громкостью звука
#define POTENTIOMETER_BRIGHT_CONTROL_PIN        35          //!< Номер пина управления яркостью светодиодной матрицы

#define ADC_POLLINGS_PERIOD                     500         //!< Период опроса каналов АЦП в количестве периодов таймера 0 (100 мкс * 500 = 50 мс)
#define ADC_CHANNELS_QUANTITY                   2           //!< Количество каналов АЦП
#define ADC_MEASUREMENTS_QUANTITY               5           //!< Количество измерений АЦП для заполнения буфера измерений
#define ADC_RESOLUTION                          12          //!< Разрядность АЦП
#define ADC_MAX_COUNTS                          4095        //!< Максимальное значение отсчётов АЦП
#define MAX_PERCENTS                            100         //!< Максимальное значение отсчётов АЦП в процентах
#define ADC_COUNTS_TO_PERCENTS_COEFF            ((float) MAX_PERCENTS / ADC_MAX_COUNTS) //!< Коэффициент для перевода отсчетов АЦП в проценты

#define POTENTIOMETER_VOLUME_CONTROL_C0         1.0252f     //!< Калибровочный коэффициент C0 для потенциометра управления громкостью звука
#define POTENTIOMETER_VOLUME_CONTROL_C1         11.5537f    //!< Калибровочный коэффициент C1 для потенциометра управления громкостью звука
#define POTENTIOMETER_BRIGHT_CONTROL_C0         1.0216f     //!< Калибровочный коэффициент C0 для потенциометра управления яркостью светодиодной матрицы
#define POTENTIOMETER_BRIGHT_CONTROL_C1         25.4780f    //!< Калибровочный коэффициент C1 для потенциометра управления яркостью светодиодной матрицы

// Пояснение. В целях упрощения было принято решение один раз
// откалибровать потенциометры с помощью метода наименьших квадратов,
// не реализовывая калибровку программно. Двухпараметрическая
// калибровка - оптимальный вариант: корректируется и наклон, и смещение.

static float adcChannelsCoeffs0[ADC_CHANNELS_QUANTITY] = { POTENTIOMETER_VOLUME_CONTROL_C0, \
                                                           POTENTIOMETER_BRIGHT_CONTROL_C0 };   //!< Массив с калибровочными коэффициентами 0 для каналов АЦП
static float adcChannelsCoeffs1[ADC_CHANNELS_QUANTITY] = { POTENTIOMETER_VOLUME_CONTROL_C1, \
                                                           POTENTIOMETER_BRIGHT_CONTROL_C1 };   //!< Массив с калибровочными коэффициентами 1 для каналов АЦП
static uint8_t adcChannels[ADC_CHANNELS_QUANTITY] = { POTENTIOMETER_VOLUME_CONTROL_PIN, \
                                                      POTENTIOMETER_BRIGHT_CONTROL_PIN };       //!< Массив с номерами пинов, у которых требуется чтение значения АЦП
static uint8_t adcCountsInPercents[ADC_CHANNELS_QUANTITY];                                      //!< Массив с отсчетами АЦП в процентах

//! \brief Инициализация АЦП
void AdcMeasurements_Init(void)
{
    // Установка разрядности АЦП для всех пинов
    analogReadResolution(ADC_RESOLUTION);
    
    // Установка диапазона измерений 0 - 3.3V для каждого пина
    for (uint8_t channelIndex = 0; channelIndex < POTENTIOMETERS_QUANTITY; channelIndex++)
    {
        analogSetPinAttenuation(adcChannels[channelIndex], ADC_11db);
    }
}

//! \brief Фильтр скользящего среднего для каналов АЦП
static void AdcMeasurements_MovingAverageFilter(void)
{
    // Массив измерений значений АЦП потенциометров
    static uint16_t adcValues[ADC_CHANNELS_QUANTITY][ADC_MEASUREMENTS_QUANTITY];
    
    // Суммарное значение АЦП каналов
    static uint32_t sumAdcValue[ADC_CHANNELS_QUANTITY];
    
    // Индекс текущего измерения АЦП
    static uint8_t measurementIndex = 0;
    
    // Счетчик заполненных измерений в буфере АЦП 
    // (счетчик заходов в функицию)
    static uint8_t measurementsCount = 0;
    
    for (uint8_t channelIndex = 0; channelIndex < ADC_CHANNELS_QUANTITY; channelIndex++)
    {
        // Получение текущего значения АЦП
        uint16_t currentAdcValue = (uint16_t) analogRead(adcChannels[channelIndex]);
        
        // Вычитание старого значения из суммарного 
        // значения АЦП, если буфер уже заполнен
        if (measurementsCount >= ADC_MEASUREMENTS_QUANTITY)
        {
            sumAdcValue[channelIndex] -= adcValues[channelIndex][measurementIndex];
        }
        
        // Сохранение нового значения АЦП
        adcValues[channelIndex][measurementIndex] = currentAdcValue;

        // Добавление нового значения в суммарное значение АЦП
        sumAdcValue[channelIndex] += currentAdcValue;
    }
    
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
    
    // Вычисляем проценты для каждого канала
    for (uint8_t channelIndex = 0; channelIndex < ADC_CHANNELS_QUANTITY; channelIndex++)
    {
        // Если выполнено, как минимум, одно измерение АЦП
        if (measurementsCount > 0)
        {
            // Вычисление усредненного значения отсчетов АЦП
            uint16_t meanAdcValue = (uint16_t) (sumAdcValue[channelIndex] / measurementsCount);

            // Вычисление усредненного значения отсчетов АЦП с калибровкой
            uint16_t meanCalibratedAdcCounts = (uint16_t) (adcChannelsCoeffs0[channelIndex] * meanAdcValue + adcChannelsCoeffs1[channelIndex]);

            // Ограничение отсчетов АЦП
            if (meanCalibratedAdcCounts > ADC_MAX_COUNTS)
            {
                meanCalibratedAdcCounts = ADC_MAX_COUNTS;
            }
            
            // Вычисление отсчетов АЦП в процентах
            adcCountsInPercents[channelIndex] = (uint8_t) (meanCalibratedAdcCounts * ADC_COUNTS_TO_PERCENTS_COEFF);
            
            // Ограничение процентов
            if (adcCountsInPercents[channelIndex] > MAX_PERCENTS)
            {
                adcCountsInPercents[channelIndex] = MAX_PERCENTS;
            }
        }
    }
}

//! \brief Периодический опрос каналов АЦП
void AdcMeasurements_Pollings(void)
{
    // Время последнего опроса АЦП
    static uint32_t lastAdcPollingTime = 0;

    // Чтение текущего положения ручек потенциометров
    // каждые ADC_POLLINGS_PERIOD периодов таймера 0
    if ((UserTimer_GetCounterTime() - lastAdcPollingTime) > ADC_POLLINGS_PERIOD)
    {
        // Фильтр скользящего среднего для каналов АЦП
        AdcMeasurements_MovingAverageFilter();

        // Обновление времени последнего опроса АЦП
        lastAdcPollingTime = UserTimer_GetCounterTime();
    }
}

//! \brief Получение адреса массива с отсчетами АЦП в процентах
//! \return Адрес массива с отсчетами АЦП в процентах
uint8_t * AdcMeasurements_GetAdcCountsInPercentsPointer(void)
{
    return adcCountsInPercents;
}