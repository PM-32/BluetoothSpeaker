#include "esp32-hal-adc.h"

#include "AdcMeasurements.h"
#include "UserTimer.h"

#define POTENTIOMETER_VOLUME_CONTROL_PIN            34          //!< Номер пина потенциометра управления громкостью звука
#define BATTERY_VOLTAGE_PIN                         35          //!< Номер пина, на котором измеряется напряжение аккумуляторной сборки

#define ADC_POLLINGS_PERIOD                         500         //!< Период опроса каналов АЦП в количестве периодов таймера 0 (100 мкс * 500 = 50 мс)
#define ADC_MEASUREMENTS_QUANTITY_POTENTIOMETER     5           //!< Размер буфера измерений канала потенциометра
#define ADC_MEASUREMENTS_QUANTITY_BATTERY           100         //!< Размер буфера измерений канала заряда батареи
#define ADC_RESOLUTION                              12          //!< Разрядность АЦП
#define ADC_MAX_COUNTS                              4095        //!< Максимальное значение отсчётов АЦП
#define ADC_REF_VOLTAGE                             3.3f        //!< Опорное напряжение АЦП
#define MAX_PERCENTS                                100         //!< Максимальное значение отсчётов АЦП в процентах
#define ADC_COUNTS_TO_PERCENTS_COEFF                ((float) MAX_PERCENTS / ADC_MAX_COUNTS) //!< Коэффициент для перевода отсчетов АЦП в проценты
#define ADC_COUNTS_TO_VOLTAGE_COEFF                 (ADC_REF_VOLTAGE / ADC_MAX_COUNTS)      //!< Коэффициент для перевода отсчетов АЦП в напряжение

#define RESISTIVE_DIVIDER_UP_ARM                    18.0f       //!< Номинал резистора верхнего плеча резистивного делителя напряжения батареи (кОм)
#define RESISTIVE_DIVIDER_LOW_ARM                   10.0f       //!< Номинал резистора нижнего плеча резистивного делителя напряжения батареи (кОм)
#define RESISTIVE_DIVIDER_COEFF                     (RESISTIVE_DIVIDER_LOW_ARM / (RESISTIVE_DIVIDER_LOW_ARM + RESISTIVE_DIVIDER_UP_ARM)) //!< Коэффициент резистивного делителя напряжения батареи
#define INVERSE_RESISTIVE_DIVIDER_COEFF             (1 / RESISTIVE_DIVIDER_COEFF)   //!< Обратное значение коэффициента резистивного делителя напряжения батареи

#define POTENTIOMETER_VOLUME_CONTROL_C0             1.0252f     //!< Калибровочный коэффициент C0 для потенциометра управления громкостью звука
#define POTENTIOMETER_VOLUME_CONTROL_C1             11.5537f    //!< Калибровочный коэффициент C1 для потенциометра управления громкостью звука        

#define CALIBRATION_POINTS_QUANTITY                 (sizeof(calibrationTable) / sizeof(calibrationTable[0]))    //!< Количество калибровочных точек 

//! \brief Каналы АЦП
typedef enum
{
    ADC_POTENTIOMETER_CHANNEL_INDEX = 0,    //!< Индекс канала АЦП, к которому подключен потенциометр
    ADC_BATTERY_CHANNEL_INDEX,              //!< Индекс канала АЦП, которому подключена батарея
    ADC_CHANNELS_QUANTITY                   //!< Количество каналов АЦП
} AdcChannels;

//! \brief Калибровочная точка для коррекции напряжения батареи
typedef struct
{
    float adcVoltage;     //!< Напряжение, рассчитанное через АЦП
    float realVoltage;    //!< Напряжение, измеренное мультиметром
} CalibrationPoint;

//! \brief Калибровочная таблица для коррекции напряжения батареи
//! \details Таблица содержит пары значений: напряжение, рассчитанное через АЦП,
//!          и реальное напряжение, измеренное мультиметром
static const CalibrationPoint calibrationTable[] =
{
    {8.198, 8.268},
    {8.073, 8.197},
    {7.949, 8.112},
    {7.751, 7.967},
    {7.672, 7.923},
    {7.525, 7.826},
    {7.322, 7.685},
    {7.214, 7.599},
    {7.108, 7.503},
    {6.972, 7.411},
    {6.729, 7.198},
    {6.559, 7.048},
    {6.392, 6.901},
    {6.255, 6.771},
    {6.106, 6.622},
    {5.957, 6.489},
    {5.770, 6.298},
    {5.549, 6.057}
};

static uint8_t adcPins[] = { POTENTIOMETER_VOLUME_CONTROL_PIN, BATTERY_VOLTAGE_PIN };   //!< Номера пинов АЦП
static uint8_t potentiometerAdcCountsInPercents = 0;        //!< Отсчеты АЦП потенциометра в процентах
static float batteryVoltage = 0.0f;                         //!< Напряжение батареи

// В целях упрощения было принято решение один раз
// откалибровать потенциометр с помощью метода наименьших квадратов,
// не реализовывая калибровку программно. Двухпараметрическая
// калибровка - оптимальный вариант: корректируется и наклон, и смещение.

// Для обеспечения автономной работы колонки (при отсутствии постоянного
// подключения к блоку питания) используется аккумуляторная сборка 2S4P на
// аккумуляторах 18650 (8 аккумуляторов по 3000 мАч). Ёмкость батареи -
// 12000 мАч, макс. напряжение - 8.4 В. Измерение оставшегося заряда
// (напряжения) батареи выполняется через канал АЦП микроконтроллера.
// Для безопасного измерения напряжения батареи используется резистивный
// делитель, понижающий напряжение до 3 В (макс. напряжение).

//! \brief Инициализация АЦП
void AdcMeasurements_Init(void)
{
    // Установка разрядности АЦП для всех пинов
    analogReadResolution(ADC_RESOLUTION);
    
    // Установка диапазона измерений 0 - 3.3V для пина потенциометра
    analogSetPinAttenuation(POTENTIOMETER_VOLUME_CONTROL_PIN, ADC_11db);

    // Установка диапазона измерений 0 - 3.3V для пина измерения заряда батареи
    analogSetPinAttenuation(BATTERY_VOLTAGE_PIN, ADC_11db);
}

//! \brief Калибровка напряжения батареи по таблице
//! \param[in] rawVoltage - напряжение, рассчитанное через АЦП
//! \return Калиброванное напряжение батареи
static float CalibrateBatteryVoltage(float rawVoltage)
{
    // Если напряжение выше первой точки таблицы - экстраполяция вверх
    if (rawVoltage >= calibrationTable[0].adcVoltage)
    {
        // Вычисление коэффициента наклона по двум первым точкам
        float slope = (calibrationTable[1].realVoltage - calibrationTable[0].realVoltage) /
                      (calibrationTable[1].adcVoltage - calibrationTable[0].adcVoltage);
        
        // Экстраполяция напряжения
        float calibratedVoltage = calibrationTable[0].realVoltage +
                                  slope * (rawVoltage - calibrationTable[0].adcVoltage);
        
        return calibratedVoltage;
    }
    
    // Если напряжение ниже последней точки таблицы - экстраполяция вниз
    if (rawVoltage <= calibrationTable[CALIBRATION_POINTS_QUANTITY - 1].adcVoltage)
    {
        // Вычисление коэффициента наклона по двум последним точкам
        float slope = (calibrationTable[CALIBRATION_POINTS_QUANTITY - 1].realVoltage -
                       calibrationTable[CALIBRATION_POINTS_QUANTITY - 2].realVoltage) /
                      (calibrationTable[CALIBRATION_POINTS_QUANTITY - 1].adcVoltage -
                       calibrationTable[CALIBRATION_POINTS_QUANTITY - 2].adcVoltage);
        
        // Экстраполяция напряжения
        float calibratedVoltage = calibrationTable[CALIBRATION_POINTS_QUANTITY - 1].realVoltage +
                                  slope * (rawVoltage - calibrationTable[CALIBRATION_POINTS_QUANTITY - 1].adcVoltage);
        
        return calibratedVoltage;
    }

    // Поиск интервала для интерполяции
    for (uint8_t pointIndex = 0; pointIndex < (CALIBRATION_POINTS_QUANTITY - 1); pointIndex++)
    {
        // Если текущее напряжение попадает в интервал между calibrationTable[pointIndex + 1] и calibrationTable[pointIndex]
        if ((rawVoltage >= calibrationTable[pointIndex + 1].adcVoltage) &&
            (rawVoltage < calibrationTable[pointIndex].adcVoltage))
        {
            // Вычисление коэффициента интерполяции
            float interpolationCoefficient = (rawVoltage - calibrationTable[pointIndex + 1].adcVoltage) /
                                             (calibrationTable[pointIndex].adcVoltage - calibrationTable[pointIndex + 1].adcVoltage);

            // Вычисление разницы напряжений
            float voltageDifference = calibrationTable[pointIndex].realVoltage - calibrationTable[pointIndex + 1].realVoltage;

            // Линейная интерполяция напряжения
            float calibratedVoltage = calibrationTable[pointIndex + 1].realVoltage +
                                      interpolationCoefficient * voltageDifference;
            
            return calibratedVoltage;
        }
    }

    return rawVoltage;
}

//! \brief Фильтр скользящего среднего для канала АЦП
static void AdcMeasurements_MovingAverageFilter(void)
{
    // Буферы для хранения измерений АЦП для каждого канала отдельно
    static uint16_t adcValuesPotentiometerChannel[ADC_MEASUREMENTS_QUANTITY_POTENTIOMETER];
    static uint16_t adcValuesBatteryChannel[ADC_MEASUREMENTS_QUANTITY_BATTERY];

    // Суммарное значение АЦП канала
    static uint32_t sumAdcValue[ADC_CHANNELS_QUANTITY];
    
    // Индекс текущего измерения АЦП
    static uint8_t measurementIndex[ADC_CHANNELS_QUANTITY];
    
    // Счетчик заполненных измерений в буфере АЦП
    // (счетчик заходов в функцию)
    static uint8_t measurementsCount[ADC_CHANNELS_QUANTITY];

    for (uint8_t indexChannel = 0; indexChannel < ADC_CHANNELS_QUANTITY; indexChannel++)
    {
        // Размер буфера для хранения измерений АЦП канала
        uint8_t channelMeasurementsQuantuty = 1;

        // Указатель на буфер для хранения измерений АЦП канала
        uint16_t *pAdcValuesChannel = NULL;

        // В зависимости от канала выбирается указатель
        // на необходимый буфер и количество измерений АЦП
        if (ADC_POTENTIOMETER_CHANNEL_INDEX == indexChannel)
        {
            channelMeasurementsQuantuty = ADC_MEASUREMENTS_QUANTITY_POTENTIOMETER;
            pAdcValuesChannel = (uint16_t *) &adcValuesPotentiometerChannel;
        }
        else if (ADC_BATTERY_CHANNEL_INDEX == indexChannel)
        {
            channelMeasurementsQuantuty = ADC_MEASUREMENTS_QUANTITY_BATTERY;
            pAdcValuesChannel = (uint16_t *) &adcValuesBatteryChannel;
        }

        // Получение текущего значения канала АЦП
        uint16_t currentAdcValue = (uint16_t) analogRead(adcPins[indexChannel]);
        
        // Вычитание старого значения из суммарного
        // значения АЦП, если буфер уже заполнен
        if (measurementsCount[indexChannel] >= channelMeasurementsQuantuty)
        {
            sumAdcValue[indexChannel] -= pAdcValuesChannel[measurementIndex[indexChannel]];
        }
        
        // Сохранение нового значения АЦП
        pAdcValuesChannel[measurementIndex[indexChannel]] = currentAdcValue;
        
        // Добавление нового значения в суммарное значение АЦП
        sumAdcValue[indexChannel] += currentAdcValue;
        
        // Увеличение индекса измерений АЦП
        measurementIndex[indexChannel]++;
        
        // Сброс индекса измерений АЦП
        if (measurementIndex[indexChannel] >= channelMeasurementsQuantuty)
        {
            measurementIndex[indexChannel] = 0;
        }
        
        // Увеличение счетчика заполненных измерений
        if (measurementsCount[indexChannel] < channelMeasurementsQuantuty)
        {
            measurementsCount[indexChannel]++;
        }
        
        // Если выполнено, как минимум, одно измерение АЦП
        if (measurementsCount[indexChannel] > 0)
        {
            // Вычисление усредненного значения отсчетов АЦП
            uint16_t meanAdcValue = (uint16_t) (sumAdcValue[indexChannel] / measurementsCount[indexChannel]);
            
            if (ADC_POTENTIOMETER_CHANNEL_INDEX == indexChannel)    // Канал АЦП для потенциометра
            {
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
            else if (ADC_BATTERY_CHANNEL_INDEX == indexChannel)     // Канал АЦП для батареи
            {
                // Фактическое измеренное напряжение батареи
                float rawBatteryVoltage = (float)(meanAdcValue * ADC_COUNTS_TO_VOLTAGE_COEFF * INVERSE_RESISTIVE_DIVIDER_COEFF);

                // Откалиброванное напряжение батареи
                batteryVoltage = CalibrateBatteryVoltage(rawBatteryVoltage);
            }
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

//! \brief Получение значения напряжения батареи
//! \return Напряжение батареи
float AdcMeasurements_GetBatteryVoltage(void)
{
    return batteryVoltage;
}