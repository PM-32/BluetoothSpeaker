#include <AudioTools.h>
#include <BluetoothA2DPSink.h>

#include "ButtonsDriver.h"
#include "UserTimer.h"

#define DEBUG_INFO_BUTTONS_STATE    // Вывод информации о состоянии кнопок
                                    // на терминал при необходимости

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
    // Вывод информации о состоянии кнопок
    // на терминал при необходимости
    #ifdef DEBUG_INFO_BUTTONS_STATE

        // Получение адреса массива с количеством устойчивых нажатий на кнопки
        uint8_t *pButtonsPressCount = ButtonsDriver_GetButtonsPressCountPointer();

        Serial.printf("Количество нажатий на кнопку управления звуком: %d\n", pButtonsPressCount[BUTTON_SOUND_CONTROL]);

    #endif // DEBUG_INFO_BUTTONS_STATE

    delay(100);
}