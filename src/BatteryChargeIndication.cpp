#include <Adafruit_NeoPixel.h>

#include "AdcMeasurements.h"
#include "BatteryChargeIndication.h"
#include "UserTimer.h"

// #define DEBUG_INFO_BATTERY_VOLTAGE                          // Вывод информации о напряжении батареи
#define MONITORING_MESSAGE_PERIOD               50000       // Период вывода сообщений в терминал в количестве периодов таймера 0 (100 мкс * 50000 = 5 с) 

#define BATTERY_CHARGE_LED_PIN                  2           //!< Пин к которому подключен RGB-светодиод WS2812 для индикации заряда батареи

#define LED_STRIP_LEDS_QUANTITY                 1           //!< Количество светодиодов
#define DEFAULT_BRIGHTNESS                      5           //!< Яркость светодиода по умолчанию (0-255)
#define COLOR_SWITCH_PERIOD_MS                  1000        //!< Период переключения цветов светодиода в мс
#define TIMER_PERIOD_MS                         0.1f        //!< Период таймера в мс (100 мкс = 0.1 мс)
#define COLOR_SWITCH_PERIOD_TICKS               ((uint32_t) (COLOR_SWITCH_PERIOD_MS / TIMER_PERIOD_MS))     //!< Период переключения цвета светодиода в тиках таймера
#define COLORS_QUANTITY                         5           //!< Количество используемых цветов
#define MAX_COLOR_VALUE                         255         //!< Максимальное значение одного цвета
#define COLOR_CHANGE_INTERVAL_IN_PERCENTS       20          //!< Интервал заряда батареи в процентах, в течение которого светодиод загорается одним цветом 
#define COLOR_CHANGE_HYSTERESIS_IN_PERCENTS     5.0f        //!< Зона нечувствительности для переключения цветов (в процентах)

#define BATTERY_MAX_VOLTAGE                     8.25f       //!< Максимальное напряжение батареи (В)
#define BATTERY_MIN_VOLTAGE                     5.80f       //!< Минимальное напряжение батареи (В)
#define BATTERY_LAST_15_PROCENTS_VOLTAGE        7.20f       //!< Напряжение батареи, при котором оставшийся заряд составляет 15% (В)
#define BATTERY_LAST_15_PROCENTS                15.0f       //!< Обозначение для точки, в которой заряд батареи составляет 15%
#define VOLTAGE_IN_PERCENTS_MAX                 100.0f      //!< Максимальное значение напяржения батареи в процентах
#define VOLTAGE_IN_PERCENTS_MIN                 0.0f        //!< Минимальное значение напяржения батареи в процентах
#define BATTERY_85_PROCENTS                     (VOLTAGE_IN_PERCENTS_MAX - BATTERY_LAST_15_PROCENTS)    //!< Участок длинной 85% заряда батареи

//! \brief Цвет светодиода
typedef struct
{
    uint8_t red;        //!< Красная составляющая цвета светодиода
    uint8_t green;      //!< Зеленая составляющая цвета светодиода
    uint8_t blue;       //!< Синяя составляющая цвета светодиода
} Color;

static Color green  = { .red = 0,               .green = MAX_COLOR_VALUE, .blue = 0 };  //!< Зеленый цвет светодиода (заряд батареи 81-100%)
static Color lime   = { .red = 100,             .green = MAX_COLOR_VALUE, .blue = 0 };  //!< Салатовый цвет светодиода (заряд батареи 61-80%)
static Color yellow = { .red = MAX_COLOR_VALUE, .green = MAX_COLOR_VALUE, .blue = 0 };  //!< Желтый цвет светодиода (заряд батареи 41-60%)
static Color orange = { .red = MAX_COLOR_VALUE, .green = 50,              .blue = 0 };  //!< Оранжевый цвет светодиода (заряд батареи 21-40%)
static Color red    = { .red = MAX_COLOR_VALUE, .green = 0,               .blue = 0 };  //!< Красный цвет светодиода (заряд батареи 0-20%)
static Color ledOff = { .red = 0,               .green = 0,               .blue = 0 };  //!< Выключение светодиода

static Color colors[COLORS_QUANTITY] = { red, orange, yellow, lime, green };    //!< Доступные цвета светодиода для индикации заряда батареи
static Adafruit_NeoPixel *pStrip = nullptr;                                     //!< Указатель на объект Adafruit_NeoPixel класса для управления WS2812

//! \brief Инициализация светодиода индикации заряда батареи
void BatteryChargeIndication_Init(void)
{
    // Создание объекта NeoPixel в динамической памяти
    pStrip = new Adafruit_NeoPixel(LED_STRIP_LEDS_QUANTITY, BATTERY_CHARGE_LED_PIN, NEO_GRB + NEO_KHZ800);
    
    // Запуск NeoPixel
    pStrip->begin();
    
    // Установка яркости по умолчанию
    pStrip->setBrightness(DEFAULT_BRIGHTNESS);
    
    // Включение светодиода
    pStrip->show();
}

//! \brief Установка цвета светодиода
//! \param[in] color - цвет светодиода
static void BatteryChargeIndication_SetColor(Color color)
{
    // Проверка существования объекта класса Adafruit_NeoPixel
    if (nullptr == pStrip)
    {
        return;
    }
    
    // Установка цвета светодиода
    pStrip->setPixelColor(0, pStrip->Color(color.red, color.green, color.blue));
    
    // Применение изменений цвета светодиода
    pStrip->show();
}

//! \brief Расчет заряда батареи в процентах
//! \param[in] batVoltage - напряжение батареи (В)
//! \return Напряжение батареи в процентах
static float CalculateBatteryVoltageInPercents(float batVoltage)
{
    // Проверка пределов напряжения батареи (0 - 100 %)
    if (batVoltage > BATTERY_MAX_VOLTAGE)
    {
        return VOLTAGE_IN_PERCENTS_MAX;
    }
    else if (batVoltage < BATTERY_MIN_VOLTAGE)
    {
        return VOLTAGE_IN_PERCENTS_MIN;
    }

    // Заряд батареи определяется по кусочно-линейной функции.
    // Это обосновывается тем, что напряжение начинает падать быстрее при
    // большом разряде батареи. Участок 100 - 15% рассчитывается по одной 
    // формуле, участок 15 - 0% по другой. Таким образом, точка 15% делит
    // расчет на две части.

    // Напряжение батареи в процентах
    float voltageInPercents = 0.0f;

    if (batVoltage >= BATTERY_LAST_15_PROCENTS_VOLTAGE) // Расчет заряда для участка 100 - 15%
    {
        voltageInPercents = BATTERY_LAST_15_PROCENTS + (batVoltage - BATTERY_LAST_15_PROCENTS_VOLTAGE) * \
                            (BATTERY_85_PROCENTS / (BATTERY_MAX_VOLTAGE - BATTERY_LAST_15_PROCENTS_VOLTAGE));
    }
    else // Расчет заряда для участка 15 - 0%
    {
        voltageInPercents = (batVoltage - BATTERY_MIN_VOLTAGE) * (BATTERY_LAST_15_PROCENTS / \
                            (BATTERY_LAST_15_PROCENTS_VOLTAGE - BATTERY_MIN_VOLTAGE));
    }

    #ifdef DEBUG_INFO_BATTERY_VOLTAGE

        // Время последней отправки сообщения в терминал
        static uint32_t lastMessageSendTime = 0;
        
        // Вывод сообщения с периодом MONITORING_MESSAGE_PERIOD
        if ((UserTimer_GetCounterTime() - lastMessageSendTime) > MONITORING_MESSAGE_PERIOD)
        {
            Serial.printf("Заряд батареи:      %.1f  %%\r\n", voltageInPercents);
            Serial.print("---------------------------\r\n");
            
            // Обновление времени последней отправки сообщения в терминал
            lastMessageSendTime = UserTimer_GetCounterTime();
        }

    #endif // DEBUG_INFO_BATTERY_VOLTAGE

    // Ограничение пределов заряда батареи в процентах (0 - 100 %)
    if (voltageInPercents > VOLTAGE_IN_PERCENTS_MAX)
    {
        return VOLTAGE_IN_PERCENTS_MAX;
    }
    else if (voltageInPercents < VOLTAGE_IN_PERCENTS_MIN)
    {
        return VOLTAGE_IN_PERCENTS_MIN;
    }

    return voltageInPercents;
}

//! \brief Определение индекса цвета для индикации заряда с учетом гистерезиса
//! \param[in] batteryVoltageInPercents - текущий заряд батареи в процентах
//! \return Индекс цвета для индикации заряда батареи
static uint8_t GetColorIndexWithHysteresis(float batteryVoltageInPercents)
{
    // Последний установленный индекс цвета
    static uint8_t lastColorIndex = 0;

    // Последний измеренный процент заряда батареи
    static float lastBatteyVoltageInPercents = -1.0f;

    // Первый вызов функции - вычисление индекса и
    // сохранение процента заряда батареи
    if (lastBatteyVoltageInPercents < 0.0f)
    {
        // Сохранение последнего процента заряда батареи
        lastBatteyVoltageInPercents = batteryVoltageInPercents;

        // Сохранение индекса последнего цвета
        lastColorIndex = (uint8_t) (batteryVoltageInPercents / COLOR_CHANGE_INTERVAL_IN_PERCENTS);

        // Ограничение индекса последнего цвета
        if (lastColorIndex >= COLORS_QUANTITY)
        {
            lastColorIndex = COLORS_QUANTITY - 1;
        }

        return lastColorIndex;
    }

    // Определение "сырого" индекса цвета
    uint8_t basicColorIndex = (uint8_t) (batteryVoltageInPercents / COLOR_CHANGE_INTERVAL_IN_PERCENTS);

    // Ограничение "сырого" индекса цвета
    if (basicColorIndex >= COLORS_QUANTITY)
    {
        basicColorIndex = COLORS_QUANTITY - 1;
    }

    // Если "сырой" индекс совпадает
    // с текущим, то ничего не меняется
    if (basicColorIndex == lastColorIndex)
    {
        // Сохранение последнего процента заряда батареи
        lastBatteyVoltageInPercents = batteryVoltageInPercents;

        return lastColorIndex;
    }

    if (basicColorIndex < lastColorIndex) // Если заряда становится меньше - переключение по порогу на следующий цвет
    {
        // Расчет порога для переключения цвета с учетом гистерезиса
        float thresholdColorChangeInPercents = (float) ((lastColorIndex * COLOR_CHANGE_INTERVAL_IN_PERCENTS) - COLOR_CHANGE_HYSTERESIS_IN_PERCENTS);

        // Проверка достижения порога для переключения цвета вниз
        if (batteryVoltageInPercents <= thresholdColorChangeInPercents)
        {
            lastColorIndex = basicColorIndex;
        }
    }
    else if (basicColorIndex > lastColorIndex) // Если заряда становится больше - переключение по порогу на следующий цвет
    {
        // Расчет порога для переключения цвета с учетом гистерезиса
        float thresholdColorChangeInPercents = (float) ((basicColorIndex * COLOR_CHANGE_INTERVAL_IN_PERCENTS) + COLOR_CHANGE_HYSTERESIS_IN_PERCENTS);

        // Проверка достижения порога для переключения цвета вниз
        if (batteryVoltageInPercents >= thresholdColorChangeInPercents)
        {
            lastColorIndex = basicColorIndex;
        }
    }

    // Сохранение последнего процента заряда батареи
    lastBatteyVoltageInPercents = batteryVoltageInPercents;

    return lastColorIndex;
}

//! \brief Индикация заряда батареи
void BatteryChargeIndication_Execute(void)
{
    // Получение текущего напряжения батареи
    float currentBatteryVoltage = AdcMeasurements_GetBatteryVoltage();

    #ifdef DEBUG_INFO_BATTERY_VOLTAGE

        // Время последней отправки сообщения в терминал
        static uint32_t lastMessageSendTime = 0;
        
        // Вывод сообщения с периодом MONITORING_MESSAGE_PERIOD
        if ((UserTimer_GetCounterTime() - lastMessageSendTime) > MONITORING_MESSAGE_PERIOD)
        {
            Serial.printf("Напряжение батареи: %.3f В\r\n", currentBatteryVoltage);
            
            // Обновление времени последней отправки сообщения в терминал
            lastMessageSendTime = UserTimer_GetCounterTime();
        }

    #endif // DEBUG_INFO_BATTERY_VOLTAGE

    // Определение заряда батареи в процентах
    float currentBatteryVoltageInPercents = CalculateBatteryVoltageInPercents(currentBatteryVoltage);

    // Определение индекса цвета для индикации заряда с учетом гистерезиса
    uint8_t colorIndex = GetColorIndexWithHysteresis(currentBatteryVoltageInPercents);

    if (colorIndex < COLORS_QUANTITY) // Проверка на допустимость индекса цвета
    {
        // Включение светодиода необходимым цветом
        BatteryChargeIndication_SetColor(colors[colorIndex]);
    }
    else // Выключение светодиода, если индекс цвета не корректен
    {
        BatteryChargeIndication_SetColor(ledOff);
    }
}