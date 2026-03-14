#include <AudioTools.h>
#include <BluetoothA2DPSink.h>

#include "ButtonsDriver.h"
#include "SoundControl.h"

// #define DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE       // Вывод информации о состоянии кнопки 
//                                                     // управления звука на терминал
#define DEBUG_INFO_PLAYBACK_CONTROL                 // Вывод информации о выполнении команд
                                                    // управления воспроизведением звука

//! \brief Текущее состояние воспроизведения звука
typedef enum
{
    PLAYPACK_PLAYING = 0,       //!< Звук воспроизводится
    PLAYBACK_PAUSED             //!< Воспроизведение звука остановлено
} PlaybackState;

static PlaybackState playbackState = PLAYPACK_PLAYING;  //!< Текущее состояние воспроизведения звука

I2SStream i2s;                  //!< Класс для I2S1
BluetoothA2DPSink a2dp_sink;    //!< Класс для реализации протокола A2DP (Bluetooth)

// Класс для ограничения максимальной громкости звука
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

// Ограничитель максимальной громкости звука
LimiterStream limiter(i2s);

//! \brief Инициализация I2S1
void SoundControl_Init(void)
{
    auto cfg = i2s.defaultConfig();     // Установка конфигурации по умолчанию
    cfg.port_no = 1;                    // Работа с I2S1
    cfg.pin_bck = 19;                   // Номер пина BCK (тактовый сигнал)
    cfg.pin_ws = 18;                    // Номер пина WS/LCK (выбор канала: правый/левый)
    cfg.pin_data = 21;                  // Номер пина DIN (аудиоданные)
    cfg.sample_rate = 44100;            // Частота дискретизации (Гц)
    cfg.bits_per_sample = 16;           // Количество бит для цифрового представления одного сэмпла (отсчета) звука
    cfg.channels = 2;                   // Количество каналов (моно = 1/стерео = 2)

    // Запуск I2S1
    i2s.begin(cfg);                     

    // Включение ограничителя максимальной громкости звука
    a2dp_sink.set_output(limiter);

    // Запуск A2DP-протокола
    a2dp_sink.start("MyMusic");
}

//! \brief Смена текущего состояния воспроизведения звука
static void SoundControl_SwitchPlaybackState(void)
{
    if (PLAYPACK_PLAYING == playbackState)
    {
        // Остановка воспроизведения звука
        a2dp_sink.pause();

        // Установка состояния - воспроизведение приостановлено
        playbackState = PLAYBACK_PAUSED;

        #ifdef DEBUG_INFO_PLAYBACK_CONTROL

            Serial.println("Воспроизведение приостановлено");

        #endif // DEBUG_INFO_PLAYBACK_CONTROL
    }
    else
    {
        // Продолжение воспроизведения звука
        a2dp_sink.play();

        // Установка состояния - воспроизведение продолжается
        playbackState = PLAYPACK_PLAYING;

        #ifdef DEBUG_INFO_PLAYBACK_CONTROL

            Serial.println("Воспроизведение продолжено");

        #endif // DEBUG_INFO_PLAYBACK_CONTROL
    }
}

//! \brief Переключение на следующий трек
static void SoundControl_NextTrack(void)
{
    // Переключение на следующий трек
    a2dp_sink.next();

    // Установка состояния - воспроизведение продолжается
    playbackState = PLAYPACK_PLAYING;

    #ifdef DEBUG_INFO_PLAYBACK_CONTROL

        Serial.println("Переключение на следующий трек");

    #endif // DEBUG_INFO_PLAYBACK_CONTROL
}

//! \brief Переключение на предыдущий трек
static void SoundControl_PreviousTrack(void)
{
    // Переключение на предыдущий трек
    a2dp_sink.previous();

    // Установка состояния - воспроизведение продолжается
    playbackState = PLAYPACK_PLAYING;

    #ifdef DEBUG_INFO_PLAYBACK_CONTROL

        Serial.println("Переключение на предыдущий трек");

    #endif // DEBUG_INFO_PLAYBACK_CONTROL
}

//! \brief Управление воспроизведением звука
//! \note Управление паузой/воспроизведением звука,
//!       переключением треков с помощью кнопки
void SoundControl_Playback(void)
{
    // Получение адреса массива с количеством устойчивых нажатий на кнопки
    uint8_t *pButtonsPressCount = ButtonsDriver_GetButtonsPressCountPointer();

    // Получение адреса массива со статусами завершения серий нажатий 
    ButtonPressSeriesStatus *pButtonSeriesStatus = ButtonsDriver_GetButtonsPressSeriesStatusPointer();

    // Если завершена серия нажатий на кнопку управления звука
    if (BUTTON_PRESS_SERIES_FINISHED == pButtonSeriesStatus[BUTTON_SOUND_CONTROL])
    {
        if (ONE_PRESS == pButtonsPressCount[BUTTON_SOUND_CONTROL])          // Зафиксировано одно нажатие на кнопку
        {
            // Смена текущего состояния звука
            SoundControl_SwitchPlaybackState();

            #ifdef DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE
            
                Serial.println("Кнопка управления звуком - действие на первое нажатие");

            #endif // DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE
        }
        else if (TWO_PRESS == pButtonsPressCount[BUTTON_SOUND_CONTROL])     // Зафиксировано два нажатия на кнопку
        {
            // Переключение на следующий трек
            SoundControl_NextTrack();

            #ifdef DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE

                Serial.println("Кнопка управления звуком - действие на второе нажатие");

            #endif // DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE
        }
        else if (THREE_PRESS == pButtonsPressCount[BUTTON_SOUND_CONTROL])   // Зафиксировано три нажатия на кнопку
        {
            // Переключение на предыдущий трек
            SoundControl_PreviousTrack();

            #ifdef DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE

                Serial.println("Кнопка управления звуком - действие на третье нажатие");

            #endif // DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE
        }

        // Сброс статуса завершения серии нажатий на кнопку управления звука
        pButtonSeriesStatus[BUTTON_SOUND_CONTROL] = BUTTON_PRESS_SERIES_NONE;

        // Сброс количества нажатий на кнопку управления звука
        pButtonsPressCount[BUTTON_SOUND_CONTROL] = 0;
    }
}