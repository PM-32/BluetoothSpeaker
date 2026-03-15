#include <AudioTools.h>
#include <BluetoothA2DPSink.h>

#include "ButtonsDriver.h"
#include "SoundControl.h"
#include "AdcMeasurements.h"
#include "UserTimer.h"

// #define DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE
// #define DEBUG_INFO_PLAYBACK_CONTROL
#define DEBUG_INFO_VOLUME_CONTROL

// Константы
#define VOLUME_TIMEOUT_PERIOD       30000                   //!< Таймаут возврата управления смартфону (3 секунды = 30000 * 100 мкс)
#define VOLUME_HYSTERESIS_PERCENT   7                       //!< Гистерезис для потенциометра (для защиты от шума)
#define AVRCP_VOLUME_MAX            127                     //!< Максимальное значение громкости в протоколе AVRCP
#define VOLUME_PERCENT_MAX          100                     //!< Максимальное значение громкости в процентах

//! \brief Текущее состояние воспроизведения звука
typedef enum
{
    PLAYBACK_PLAYING = 0,                                   //!< Звук воспроизводится
    PLAYBACK_PAUSED                                         //!< Воспроизведение звука остановлено
} PlaybackState;

//! \brief Источник управления громкостью
typedef enum
{
    VOLUME_SOURCE_PHONE = 0,                                //!< Громкость задана смартфоном
    VOLUME_SOURCE_POT                                       //!< Громкость задана потенциометром
} VolumeSource;

static PlaybackState playbackState = PLAYBACK_PLAYING;      //!< Текущее состояние воспроизведения звука
static uint8_t currentVolumePercent = 50;                   //!< Текущая громкость (0-100%)
static uint8_t lastPotPosition = 255;                       //!< Последнее положение потенциометра
static VolumeSource volumeSource = VOLUME_SOURCE_PHONE;     //!< Текущий источник громкости
static uint32_t lastPotChangeTime = 0;                      //!< Время последнего изменения потенциометра (в периодах таймера)
static uint8_t lastPhoneVolume = 255;                       //!< Последняя громкость со смартфона

I2SStream i2s;                                              //!< Класс для I2S1
BluetoothA2DPSink a2dp_sink;                                //!< Класс для реализации протокола A2DP (Bluetooth)

//! \brief Класс для регулировки громкости звука
class VolumeControlStream: public AudioStream
{
  public:
    VolumeControlStream(AudioStream &out): _out(out) {}

    size_t write(const uint8_t *data, size_t len) override
    {
        int16_t *samples = (int16_t*) data;
        size_t sample_count = len / 2;
        
        // Вычисление коэффициента громкости (0.0 - 1.0)
        float volumeFactor = (float)_volumePercent / 100.0f;

        for (size_t i = 0; i < sample_count; i++)
        {
            // Масштабирование сэмпла в соответствии с громкостью
            float scaled = samples[i] * volumeFactor;
            
            // Ограничение для 16-bit аудио
            if (scaled > 32767) scaled = 32767;
            if (scaled < -32768) scaled = -32768;
            
            samples[i] = (int16_t)scaled;
        }

        return _out.write(data, len);
    }

    //! \brief Установка громкости
    //! \param percent Уровень громкости в процентах (0-100)
    void setVolume(uint8_t percent)
    {
        if (percent > 100) percent = 100;
        _volumePercent = percent;
    }

  private:
    AudioStream &_out;              //!< Ссылка на выходной поток
    uint8_t _volumePercent = 100;   //!< Текущая громкость в процентах
};

VolumeControlStream volumeStream(i2s);

//! \brief Легковесный callback для изменения громкости на смартфоне
//! \param volume Новое значение громкости в формате AVRCP (0-127)
static void onVolumeChange(int volume)
{
    // Только сохраняем значение, ничего больше не делаем
    lastPhoneVolume = (uint8_t)((volume * 100UL) / AVRCP_VOLUME_MAX);
}

//! \brief Инициализация I2S1 и A2DP
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
    
    // Подключение регулятора громкости
    a2dp_sink.set_output(volumeStream);
    
    // Регистрация легковесного callback для получения изменений громкости со смартфона
    a2dp_sink.set_avrc_rn_volumechange(onVolumeChange);
    
    // Запуск A2DP-протокола
    a2dp_sink.start("MyMusic");
    
    // Получение текущей громкости со смартфона при старте
    int currentVolume = a2dp_sink.get_volume();
    if (currentVolume >= 0)
    {
        currentVolumePercent = (uint8_t)((currentVolume * 100UL) / AVRCP_VOLUME_MAX);
        lastPhoneVolume = currentVolumePercent;
        volumeStream.setVolume(currentVolumePercent);
    }
}

//! \brief Смена текущего состояния воспроизведения звука
static void SoundControl_SwitchPlaybackState(void)
{
    if (PLAYBACK_PLAYING == playbackState)
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
        playbackState = PLAYBACK_PLAYING;

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
    playbackState = PLAYBACK_PLAYING;

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
    playbackState = PLAYBACK_PLAYING;

    #ifdef DEBUG_INFO_PLAYBACK_CONTROL

        Serial.println("Переключение на предыдущий трек");

    #endif // DEBUG_INFO_PLAYBACK_CONTROL
}

//! \brief Регулировка громкости звука
void SoundControl_Volume(void)
{
    // Обработка громкости со смартфона
    if (volumeSource == VOLUME_SOURCE_PHONE)
    {
        // Если получено новое значение со смартфона и оно отличается от текущего
        if (lastPhoneVolume <= 100 && lastPhoneVolume != currentVolumePercent)
        {
            currentVolumePercent = lastPhoneVolume;
            volumeStream.setVolume(currentVolumePercent);
            
            #ifdef DEBUG_INFO_VOLUME_CONTROL

                Serial.printf("Громкость со смартфона: %u%%\r\n", currentVolumePercent);

            #endif // DEBUG_INFO_VOLUME_CONTROL
        }
    }

    // Получение текущего значения громкости с потенциометра
    uint8_t *pAdcCountsInPercents = AdcMeasurements_GetAdcCountsInPercentsPointer();
    uint8_t currentPotPosition = pAdcCountsInPercents[POTENTIOMETER_VOLUME_CONTROL];
    
    // Проверка таймаута потенциометра (3 секунды бездействия)
    if (volumeSource == VOLUME_SOURCE_POT)
    {
        // Если потенциометр не вращался более VOLUME_TIMEOUT_PERIOD
        if ((UserTimer_GetCounterTime() - lastPotChangeTime) > VOLUME_TIMEOUT_PERIOD)
        {
            // Возврат управления смартфону
            volumeSource = VOLUME_SOURCE_PHONE;
            
            // Отправляем текущую громкость на смартфон, чтобы синхронизировать
            uint8_t avrcpVolume = (currentVolumePercent * AVRCP_VOLUME_MAX) / 100;
            a2dp_sink.set_volume(avrcpVolume);
            
            // Обновляем последнее значение со смартфона
            lastPhoneVolume = currentVolumePercent;
            
            #ifdef DEBUG_INFO_VOLUME_CONTROL

                Serial.printf("Таймаут потенциометра - возврат к управлению со смартфона, громкость сохранена: %u%%\r\n", currentVolumePercent);

            #endif // DEBUG_INFO_VOLUME_CONTROL
        }
    }
    
    // Проверка изменения положения потенциометра с гистерезисом
    int16_t potDifference = abs((int16_t)currentPotPosition - (int16_t)lastPotPosition);
    
    // Если изменение превышает порог гистерезиса
    if (potDifference > VOLUME_HYSTERESIS_PERCENT)
    {
        // Обновление последнего положения потенциометра
        lastPotPosition = currentPotPosition;
        
        // Потенциометр становится активным источником
        volumeSource = VOLUME_SOURCE_POT;
        lastPotChangeTime = UserTimer_GetCounterTime();
        currentVolumePercent = currentPotPosition;
        
        // Применение новой громкости
        volumeStream.setVolume(currentVolumePercent);
        
        // Отправка новой громкости на смартфон
        uint8_t avrcpVolume = (currentVolumePercent * AVRCP_VOLUME_MAX) / 100;
        a2dp_sink.set_volume(avrcpVolume);
        
        #ifdef DEBUG_INFO_VOLUME_CONTROL

            Serial.printf("Громкость с потенциометра: %u%% (изменение: %d)\r\n", 
                         currentVolumePercent, potDifference);

        #endif // DEBUG_INFO_VOLUME_CONTROL
    }
}

//! \brief Управление воспроизведением звука
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