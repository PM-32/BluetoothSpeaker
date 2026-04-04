#include <Adafruit_NeoPixel.h>

#include "BatteryChargeIndication.h"
#include "UserTimer.h"

#define BATTERY_CHARGE_LED_PIN          2           //!< Пин к которому подключен RGB-светодиод WS2812 для индикации заряда батареи

#define LED_STRIP_LEDS_QUANTITY         1           //!< Количество светодиодов
#define DEFAULT_BRIGHTNESS              10          //!< Яркость светодиода по умолчанию (0-255)
#define COLOR_SWITCH_PERIOD_MS          1000        //!< Период переключения цветов светодиода в мс
#define TIMER_PERIOD_MS                 0.1f        //!< Период таймера в мс (100 мкс = 0.1 мс)
#define COLOR_SWITCH_PERIOD_TICKS       ((uint32_t) (COLOR_SWITCH_PERIOD_MS / TIMER_PERIOD_MS))     //!< Период переключения цвета светодиода в тиках таймера
#define COLORS_QUANTITY                 5           //!< Количество используемых цветов
#define MAX_COLOR_VALUE                 255         //!< Максимальное значение одного цвета

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

static Color colors[COLORS_QUANTITY] = { green, lime, yellow, orange, red };    //!< Доступные цвета светодиода для индикации заряда батареи
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

//! \brief Выключение светодиода индикации заряда батареи
void BatteryChargeIndication_LedOff(void)
{
    BatteryChargeIndication_SetColor(ledOff);
}

//! \brief Обновление цвета светодиода индикации заряда батареи
//! \note Временная проверка работы светодиода -
//!       переключение 5 цветов с заданным периодом
void BatteryChargeIndication_UpdateColor(void)
{
    // Время последней смены цвета (в тиках таймера)
    static uint32_t lastColorChangeTime = 0;

    // Индекс текущего цвета светодиода
    static uint8_t colorIndex = 0;

    // Проверка существования объекта класса Adafruit_NeoPixel
    if (nullptr == pStrip)
    {
        return;
    }
    
    // Получение текущего времени в тиках таймера
    uint32_t currentTime = UserTimer_GetCounterTime();
    
    // Проверка, прошёл ли период переключения цветов
    if ((currentTime - lastColorChangeTime) >= COLOR_SWITCH_PERIOD_TICKS)
    {
        // Установка цвета светодиода
        BatteryChargeIndication_SetColor(colors[colorIndex]);

        // Инкремент индекса текущего
        // цвета светодиода (смена цвета)
        colorIndex++;
        
        // Сброс индекса текущего цвета светодиода
        // при достижении COLORS_QUANTITY
        if (colorIndex >= COLORS_QUANTITY)
        {
            colorIndex = 0;
        }

        // Обновление времени последней смены цвета
        lastColorChangeTime = currentTime;
    }
}