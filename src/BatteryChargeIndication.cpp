#include "BatteryChargeIndication.h"
#include "UserTimer.h"
#include <Adafruit_NeoPixel.h>

#define LED_STRIP_LEDS_QUANTITY         1           //!< Количество светодиодов в ленте (1 шт.)
#define DEFAULT_BRIGHTNESS              50          //!< Яркость светодиода по умолчанию (0-255)
#define COLOR_SWITCH_PERIOD_MS          1000        //!< Период переключения цветов в миллисекундах
#define TIMER_PERIOD_MS                 0.1f        //!< Период таймера в миллисекундах (100 мкс = 0.1 мс)

// Пересчёт периода переключения из миллисекунд в тики таймера
#define COLOR_SWITCH_PERIOD_TICKS       ((uint32_t)(COLOR_SWITCH_PERIOD_MS / TIMER_PERIOD_MS))

static Adafruit_NeoPixel* pStrip = nullptr;                                     //!< Указатель на объект для управления WS2812
static BatteryChargeColor currentColor = BATTERY_COLOR_RED;                     //!< Текущий цвет индикации
static uint32_t lastColorChangeTime = 0;                                        //!< Время последней смены цвета (в тиках таймера)

//! \brief Инициализация индикации заряда батареи
//! \param[in] pin - номер пина GPIO для подключения светодиода
void BatteryChargeIndication_Init(uint8_t pin)
{
    // Создание объекта NeoPixel в динамической памяти
    pStrip = new Adafruit_NeoPixel(LED_STRIP_LEDS_QUANTITY, pin, NEO_GRB + NEO_KHZ800);
    
    // Запуск NeoPixel
    pStrip->begin();
    
    // Установка яркости (для предотвращения ослепления)
    pStrip->setBrightness(DEFAULT_BRIGHTNESS);
    
    // Выключение светодиода
    pStrip->show();
    
    // Сохранение текущего времени
    lastColorChangeTime = UserTimer_GetCounterTime();
}

//! \brief Установка цвета светодиода по компонентам RGB
//! \param[in] red - компонент красного (0-255)
//! \param[in] green - компонент зелёного (0-255)
//! \param[in] blue - компонент синего (0-255)
void BatteryChargeIndication_SetRgbColor(uint8_t red, uint8_t green, uint8_t blue)
{
    // Выход, если модуль не инициализирован
    if (nullptr == pStrip)
    {
        return;
    }
    
    // Установка цвета для единственного светодиода
    pStrip->setPixelColor(0, pStrip->Color(red, green, blue));
    
    // Применение изменений
    pStrip->show();
}

//! \brief Установка цвета индикации по предустановленному значению
//! \param[in] color - цвет из перечисления BatteryChargeColor
void BatteryChargeIndication_SetColor(BatteryChargeColor color)
{
    // Выбор цвета в зависимости от переданного значения
    switch (color)
    {
        case BATTERY_COLOR_RED:
            BatteryChargeIndication_SetRgbColor(255, 0, 0);
            break;
            
        case BATTERY_COLOR_YELLOW:
            BatteryChargeIndication_SetRgbColor(255, 255, 0);
            break;
            
        case BATTERY_COLOR_GREEN:
            BatteryChargeIndication_SetRgbColor(0, 255, 0);
            break;
            
        case BATTERY_COLOR_OFF:
            BatteryChargeIndication_SetRgbColor(0, 0, 0);
            break;
            
        default:
            BatteryChargeIndication_SetRgbColor(0, 0, 0);
            break;
    }
    
    // Сохранение текущего цвета
    currentColor = color;
}

//! \brief Периодическое обновление состояния индикации
void BatteryChargeIndication_Update(void)
{
    // Выход, если модуль не инициализирован
    if (nullptr == pStrip)
    {
        return;
    }
    
    // Получение текущего времени в тиках таймера
    uint32_t currentTime = UserTimer_GetCounterTime();
    
    // Проверка, прошёл ли период переключения цветов
    if ((currentTime - lastColorChangeTime) >= COLOR_SWITCH_PERIOD_TICKS)
    {
        // Циклическое переключение цветов: красный -> жёлтый -> зелёный -> красный -> ...
        switch (currentColor)
        {
            case BATTERY_COLOR_RED:
                BatteryChargeIndication_SetColor(BATTERY_COLOR_YELLOW);
                break;
                
            case BATTERY_COLOR_YELLOW:
                BatteryChargeIndication_SetColor(BATTERY_COLOR_GREEN);
                break;
                
            case BATTERY_COLOR_GREEN:
                BatteryChargeIndication_SetColor(BATTERY_COLOR_RED);
                break;
                
            default:
                BatteryChargeIndication_SetColor(BATTERY_COLOR_RED);
                break;
        }
        
        // Обновление времени последней смены цвета
        lastColorChangeTime = currentTime;
    }
}