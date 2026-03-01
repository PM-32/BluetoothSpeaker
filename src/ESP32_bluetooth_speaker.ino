#include <AudioTools.h>
#include <BluetoothA2DPSink.h>

#include "ButtonsDriver.h"
#include "UserTimer.h"

#define DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE       // Вывод информации о состоянии кнопки 
                                                    // управления звука на терминал при необходимости
// #define DEBUG_INFO_BUTTON_INIT_BLUETOOTH_STATE      // Вывод информации о состоянии кнопки
//                                                     // инициализации Bluetooth на терминал при необходимости

I2SStream i2s;
BluetoothA2DPSink a2dp_sink;

// Минимальный поток для обработки звука
class LimiterStream: public AudioStream
{
  public:
    LimiterStream(AudioStream &out): _out(out) {}

    size_t write(const uint8_t *data, size_t len) override
    {
        int16_t *samples = (int16_t*) data;
        size_t sample_count = len / 2;

        for (size_t i = 0; i < sample_count; i++)
        {
            samples[i] = applyLimiter(samples[i], _limit);
        }

        return _out.write(data, len);
    }   

    void setLimit(int16_t newLimit)
    {
        _limit = newLimit;
    }

  private:
    AudioStream &_out;
    int16_t _limit = 3000;

    int16_t applyLimiter(int16_t sample, int16_t limit)
    {
        // Зона мягкого срабатывания начинается раньше
        const int16_t soft_limit = limit * 0.8;

        if (sample > soft_limit)
        {
            if (sample > limit)
            {
                return limit + (sample - limit) / 4;
            } 
            else
            {
                // В зоне между soft_limit и limit - применяем плавное сжатие
                // Это создает небольшое "колено", сглаживающий вход в лимитер.
                float factor = 0.8 + 0.2 * ((float) (sample - soft_limit) / (limit - soft_limit));
                return soft_limit + (sample - soft_limit) * factor;
            }
        } 
        else if (sample < -soft_limit)
        {
            if (sample < -limit)
            {
                return -limit + (sample + limit) / 4;
            }
            else
            {
                float factor = 0.8 + 0.2 * ((float) (-sample - soft_limit) / (limit - soft_limit));
                return -soft_limit - (-sample - soft_limit) * factor;
            }
        }
        return sample;
    }
};

LimiterStream limiter(i2s);

//! \brief Функция для инициализации всего
void setup()
{
    // Запуск UART для вывода отладочной информации
    Serial.begin(9600);
    
    // Настройка пина кнопки управления звуком
    pinMode(BUTTON_SOUND_CONTROL_PIN, INPUT_PULLUP);

    // Настройка пина кнопки для инициализации Bluetooth
    pinMode(BUTTON_INIT_BLUETOOTH_PIN, INPUT_PULLUP);

    // Инициализация таймера для отсчета интервалов
    UserTimer_InitTimer();

    // Запуск таймера 0
    UserTimer_StartTim0();

    auto cfg = i2s.defaultConfig();
    cfg.pin_bck = 26;
    cfg.pin_ws = 27;
    cfg.pin_data = 25;
    cfg.sample_rate = 44100;
    cfg.bits_per_sample = 16;
    cfg.channels = 2;
    i2s.begin(cfg);

    a2dp_sink.set_output(limiter);
    a2dp_sink.start("MyMusic");
}

//! \brief Основной цикл программы
void loop()
{
    // Получение адреса массива с количеством устойчивых нажатий на кнопки
    uint8_t *pButtonsPressCount = ButtonsDriver_GetButtonsPressCountPointer();

    // Получение адреса массива со статусами завершения серий нажатий 
    ButtonPressSeriesStatus *pButtonSeriesStatus = ButtonsDriver_GetButtonsPressSeriesStatusPointer();

    // Вывод информации о состоянии кнопки 
    // управления звука на терминал при необходимости
    #ifdef DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE

        // Если завершена серия нажатий на кнопку управления звука
        if (BUTTON_PRESS_SERIES_FINISHED == pButtonSeriesStatus[BUTTON_SOUND_CONTROL])
        {
            if (ONE_PRESS == pButtonsPressCount[BUTTON_SOUND_CONTROL])          // Зафиксировано одно нажатие на кнопку
            {
                Serial.println("Кнопка управления звуком - действие на первое нажатие");
            }
            else if (TWO_PRESS == pButtonsPressCount[BUTTON_SOUND_CONTROL])     // Зафиксировано два нажатия на кнопку
            {
                Serial.println("Кнопка управления звуком - действие на второе нажатие");
            }
            else if (THREE_PRESS == pButtonsPressCount[BUTTON_SOUND_CONTROL])   // Зафиксировано три нажатия на кнопку
            {
                Serial.println("Кнопка управления звуком - действие на третье нажатие");
            }

            // Сброс статуса завершения серии нажатий на кнопку управления звука
            pButtonSeriesStatus[BUTTON_SOUND_CONTROL] = BUTTON_PRESS_SERIES_NONE;

            // Сброс количества нажатий на кнопку управления звука
            pButtonsPressCount[BUTTON_SOUND_CONTROL] = 0;
        }

    #endif  // DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE

    // Вывод информации о состоянии кнопки
    // инициализации Bluetooth на терминал при необходимости
    #ifdef DEBUG_INFO_BUTTON_INIT_BLUETOOTH_STATE
    
        // Если завершена серия нажатий на кнопку управления звука
        if (BUTTON_PRESS_SERIES_FINISHED == pButtonSeriesStatus[BUTTON_INIT_BLUETOOTH])
        {
            if (ONE_PRESS == pButtonsPressCount[BUTTON_INIT_BLUETOOTH])      // Зафиксировано одно нажатие на кнопку
            {
                Serial.println("Кнопка инициализации Bluetooth - действие на первое нажатие");
            }
            else if (TWO_PRESS == pButtonsPressCount[BUTTON_INIT_BLUETOOTH]) // Зафиксировано два нажатия на кнопку
            {
                Serial.println("Кнопка инициализации Bluetooth - действие на второе нажатие");
            }

            // Сброс статуса завершения серии нажатий на кнопку управления звука
            pButtonSeriesStatus[BUTTON_INIT_BLUETOOTH] = BUTTON_PRESS_SERIES_NONE;

            // Сброс количества нажатий на кнопку управления звука
            pButtonsPressCount[BUTTON_INIT_BLUETOOTH] = 0;
        }

    #endif // DEBUG_INFO_BUTTON_INIT_BLUETOOTH_STATE
    
    delay(100);
}