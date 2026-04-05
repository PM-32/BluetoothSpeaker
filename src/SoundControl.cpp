#include <math.h>

#include "ButtonsDriver.h"
#include "SoundControl.h"
#include "SoundPresets.h"
#include "AdcMeasurements.h"
#include "UserTimer.h"

// #define DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE           // Вывод информации о нажатии на кнопку управления звуком
// #define DEBUG_INFO_PLAYBACK_CONTROL                     // Вывод информации об управлении воспроизведением
// #define DEBUG_INFO_VOLUME_CONTROL                       // Вывод информации о регулировке громкости на терминал 

#define VOLUME_TIMEOUT_PERIOD           30000           //!< Таймаут возврата управления смартфону (3 секунды = 30000 * 100 мкс)
#define DELTA_VOLUME_THRESHOLD_VALUE    7               //!< Пороговое значение для определения изменения положения потенциометра в процентах
#define MAX_VOLUME_AVRCP                127             //!< Максимальное значение громкости в протоколе AVRCP
#define MAX_VOLUME_PERCENT              100             //!< Максимальное значение громкости в процентах
#define TARGET_MAX_VOLUME_SAMPLE        30000           //!< Целевое максимальное значение сэмпла при 100% громкости
#define LIMITER_COEFF                   ((float) TARGET_MAX_VOLUME_SAMPLE / MAX_VOLUME_SAMPLE)  //!< Коэффициент для ограничения максимальной громкости
#define VOLUME_CURVE_SHAPE              0.4f            //!< Форма кривой для регулировки громкости

// Ограничения для защиты динамиков
#define MAX_SAFE_VOLUME_PERCENT         80              //!< Максимальная безопасная громкость для агрессивных пресетов (%)
#define MAX_LIMITED_VOLUME_PERCENT      90              //!< Абсолютный максимум громкости (%)

//! \brief Текущее состояние воспроизведения звука
typedef enum
{
    PLAYBACK_PLAYING = 0,                               //!< Звук воспроизводится
    PLAYBACK_PAUSED                                     //!< Воспроизведение звука остановлено
} PlaybackState;

//! \brief Источник управления громкостью звука
typedef enum
{
    VOLUME_CONTROL_SOURCE_PHONE = 0,                    //!< Громкость звука задана смартфоном
    VOLUME_CONTROL_SOURCE_POTENTIOMETER                 //!< Громкость звука задана потенциометром
} VolumeControlSource;

static PlaybackState playbackState = PLAYBACK_PLAYING;                          //!< Текущее состояние воспроизведения звука
static VolumeControlSource volumeControlSource = VOLUME_CONTROL_SOURCE_PHONE;   //!< Текущий источник управления громкостью звука
static uint32_t lastPotentiometerChangePositionTime = 0;                        //!< Время последнего изменения положения ручки
                                                                                //!< потенциометра управления громкостью звука
static uint8_t currentVolumeInPercents = 50;                                    //!< Текущая громкость звука (0 - 100%)
static uint8_t lastPotentiometerPositionInPercents = 255;                       //!< Последнее положение ручки потенциометра
                                                                                //!< (255 - невалидное значение для первого запуска)
                                                                                //!< управления громкостью звука
static uint8_t lastPhoneVolumeInPercents = 255;                                 //!< Последняя громкость звука, установленная на смартфоне
                                                                                //!< (255 - невалидное значение для первого запуска)

I2SStream i2s;                                          //!< Класс для I2S1
BluetoothA2DPSink a2dp_sink;                            //!< Класс для реализации протокола A2DP (Bluetooth)

//! \brief Класс для регулировки громкости звука
class VolumeControlStream: public AudioStream
{
  public:

    // Поток управления аудиоданными
    VolumeControlStream(AudioStream &out): audioOut(out) {}

    // Функция для записи звука в I2S
    size_t write(const uint8_t *audioData, size_t lengthData) override
    {
        // Указатель на данные (сэмплы)
        int16_t *samples = (int16_t*) audioData;

        // Количество сэмплов
        size_t samplesQuantity = lengthData / AUDIO_CHANNELS_QUANTITY;
        
        // Вычисление коэффициента громкости с учетом ограничителя
        float volumeScaleCoeff = volumeCoeff * LIMITER_COEFF;

        for (size_t sampleIndex = 0; sampleIndex < samplesQuantity; sampleIndex++)
        {
            // Определение канала (четные - левый, нечетные - правый)
            uint8_t channel = (0 == sampleIndex % 2) ? 0 : 1;
            
            // Применение эквалайзера к сэмплу
            int16_t equalizerSample = SoundPresets_ProcessSample(samples[sampleIndex], channel);
            
            // Масштабирование громкости сэмпла
            float scaledVolumeSample = (float) equalizerSample * volumeScaleCoeff;
            
            // Ограничение громкости для 16-ти битного сэмпла
            if (scaledVolumeSample > MAX_VOLUME_SAMPLE)
            {
                scaledVolumeSample = MAX_VOLUME_SAMPLE;
            }
            else if (scaledVolumeSample < MIN_VOLUME_SAMPLE) 
            {
                scaledVolumeSample = MIN_VOLUME_SAMPLE;
            }

            // Сохранение нового сэмпла с отмасштабированной громкостью
            samples[sampleIndex] = (int16_t) scaledVolumeSample;
        }

        return audioOut.write(audioData, lengthData);
    }

    //! \brief Установка громкости звука
    //! \param percents - уровень громкости звука в процентах (0 - 100%)
    void SetVolume(uint8_t percents)
    {
        // Ограничение громкости звука в процентах
        if (percents > MAX_VOLUME_PERCENT)
        {
            percents = MAX_VOLUME_PERCENT;
        }

        // Примечание. Для регулировки громкости используется корневая
        // функция. Она нужна для быстрого роста громкости в начале
        // диапазона. Это компенсирует логарифмическое восприятие слуха 

        // Нормализованное значение громкости
        float normalizedVolume = (float) percents / MAX_VOLUME_PERCENT;

        // Расчет коэффициента громкости
        volumeCoeff = powf(normalizedVolume, VOLUME_CURVE_SHAPE);
    }

  private:

    // Ссылка на выходной поток аудиоданных
    AudioStream &audioOut;
    
    // Коэффициент громкости (0.0 - 1.0)
    float volumeCoeff = 1.0f;
};

VolumeControlStream volumeStream(i2s);  //!< Класс для управления громкостью звука

//! \brief Защита от опасной комбинации громкости и пресета
static void VolumeProtectionCheck(uint8_t volumePercent)
{
    // Получение текущего пресета
    EqualizerPreset currentPreset = SoundPresets_GetCurrentPreset();
    
    // Если выбран опасный пресет (BASS или ELECTRO)
    // и громкость превышает безопасный уровень
    if ((EQUALIZER_PRESET_BASS == currentPreset || EQUALIZER_PRESET_ELECTRO == currentPreset) &&
        (volumePercent > MAX_SAFE_VOLUME_PERCENT))
    {
        #ifdef DEBUG_INFO_VOLUME_CONTROL

            Serial.printf("Защита: громкость %u%% превышает безопасный уровень %u%% для пресета %s. Переключение на NORMAL.\r\n",
                          volumePercent, MAX_SAFE_VOLUME_PERCENT,
                          (EQUALIZER_PRESET_BASS == currentPreset) ? "BASS" : "ELECTRO");

        #endif // DEBUG_INFO_VOLUME_CONTROL
        
        // Переключение на безопасный пресет
        SoundPresets_SetPreset(EQUALIZER_PRESET_NORMAL);
    }
}

//! \brief Коллбэк-функция прерывания по изменению громкости звука со смартфона
//! \param[in] volume - значение громкости в формате
//!            AVRCP (0 - 127), установленное со смартфона
static void VolumeChangeCallback(int volume)
{
    // Ограничение максимальной громкости на уровне 90%
    if (volume > (MAX_LIMITED_VOLUME_PERCENT * MAX_VOLUME_AVRCP / MAX_VOLUME_PERCENT))
    {
        volume = (MAX_LIMITED_VOLUME_PERCENT * MAX_VOLUME_AVRCP / MAX_VOLUME_PERCENT);
    }
    
    // Сохранение значения громкости звука,
    // установленной со смартфона, в процентах
    lastPhoneVolumeInPercents = (uint8_t) ((volume * MAX_VOLUME_PERCENT) / MAX_VOLUME_AVRCP);
}

//! \brief Инициализация I2S1 и протокола A2DP
void SoundControl_Init(void)
{
    auto cfg = i2s.defaultConfig();             // Установка конфигурации по умолчанию
    cfg.port_no = 1;                            // Работа с I2S1
    cfg.pin_bck = 19;                           // Номер пина BCK (тактовый сигнал)
    cfg.pin_ws = 18;                            // Номер пина WS/LCK (выбор канала: правый/левый)
    cfg.pin_data = 21;                          // Номер пина DIN (аудиоданные)
    cfg.sample_rate = SAMPLE_RATE;              // Частота дискретизации (Гц)
    cfg.bits_per_sample = 16;                   // Количество бит для цифрового представления одного сэмпла (отсчета) звука
    cfg.channels = AUDIO_CHANNELS_QUANTITY;     // Количество каналов (моно = 1/стерео = 2)

    // Запуск I2S1
    i2s.begin(cfg);
    
    // Включение регулировки громкости
    a2dp_sink.set_output(volumeStream);
    
    // Регистрация коллбэк-функции для прерывания
    // по изменению громкости со смартфона
    a2dp_sink.set_avrc_rn_volumechange(VolumeChangeCallback);
    
    // Запуск A2DP-протокола
    a2dp_sink.start(SPEAKER_NAME);
    
    // Получение громкости звука со смартфона при старте
    uint8_t startVolume = (uint8_t) a2dp_sink.get_volume();

    // Ограничение стартовой громкости
    if (startVolume > (MAX_LIMITED_VOLUME_PERCENT * MAX_VOLUME_AVRCP / MAX_VOLUME_PERCENT))
    {
        startVolume = (MAX_LIMITED_VOLUME_PERCENT * MAX_VOLUME_AVRCP / MAX_VOLUME_PERCENT);
    }

    // Вычисление стартовой громкости в процентах
    currentVolumeInPercents = (uint8_t) ((startVolume * MAX_VOLUME_PERCENT) / MAX_VOLUME_AVRCP);

    // Сохранение значения громкости звука,
    // установленной со смартфона, в процентах
    lastPhoneVolumeInPercents = currentVolumeInPercents;

    // Установка громкости звука со смартфона
    volumeStream.SetVolume(currentVolumeInPercents);

    // Инициализация модуля звуковых пресетов
    SoundPresets_Init();
}

//! \brief Смена текущего состояния воспроизведения звука
static void SoundControl_SwitchPlaybackState(void)
{
    // Если звук воспроизводился
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
    else // Если звук не воспроизводился
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
static void SoundControl_SwitchNextTrack(void)
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
static void SoundControl_SwitchPreviousTrack(void)
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
    // Если текущим источником установки громкости звука является смартфон
    if (volumeControlSource == VOLUME_CONTROL_SOURCE_PHONE)
    {
        // Если получено новое значение громкости со
        // смартфона и оно отличается от предыдущего
        if ((lastPhoneVolumeInPercents <= MAX_LIMITED_VOLUME_PERCENT) &&
            (lastPhoneVolumeInPercents != currentVolumeInPercents))
        {
            // Сохранение текущей громкости звука
            currentVolumeInPercents = lastPhoneVolumeInPercents;
            
            // Защита от опасной комбинации громкости и пресета
            VolumeProtectionCheck(currentVolumeInPercents);
            
            // Установка громкости звука
            volumeStream.SetVolume(currentVolumeInPercents);
            
            #ifdef DEBUG_INFO_VOLUME_CONTROL

                Serial.printf("Громкость, установленная смартфоном: %u%%\r\n", currentVolumeInPercents);

            #endif // DEBUG_INFO_VOLUME_CONTROL
        }
    }

    // Если текущим источником установки громкости звука является потенциометр
    if (volumeControlSource == VOLUME_CONTROL_SOURCE_POTENTIOMETER)
    {
        // Если потенциометр не вращался более VOLUME_TIMEOUT_PERIOD
        if ((UserTimer_GetCounterTime() - lastPotentiometerChangePositionTime) > VOLUME_TIMEOUT_PERIOD)
        {
            // Возврат управления громкостью звука к смартфону
            volumeControlSource = VOLUME_CONTROL_SOURCE_PHONE;
            
            // Расчет громкости звука в рамках протокола AVRCP
            uint8_t avrcpVolume = (currentVolumeInPercents * MAX_VOLUME_AVRCP) / MAX_VOLUME_PERCENT;

            // Обновление текущей громкости звука
            // смартфоне для синхронизации значения
            a2dp_sink.set_volume(avrcpVolume);
            
            // Обновление последнего значения со смартфона
            lastPhoneVolumeInPercents = currentVolumeInPercents;
            
            #ifdef DEBUG_INFO_VOLUME_CONTROL

                Serial.printf("Управление громкостью вернулось к смартфону, текущая громкость: %u%%\r\n", currentVolumeInPercents);

            #endif // DEBUG_INFO_VOLUME_CONTROL
        }
    }

    // Получение адреса массива с отсчетами АЦП в процентах
    uint8_t *pAdcCountsInPercents = AdcMeasurements_GetAdcCountsInPercentsPointer();
    
    // Пояснение. Значения АЦП, приходящие с каналов, к которым подключены
    // потенциометры изменяются в большом диапазоне даже в случае, когда
    // потенциометр не вращается. Чтобы случайные изменения в значениях АЦП
    // не приводили к изменению громкости звука, была добавлена защита.
    // Изменение громкости с потенциометра фиксируется только, когда громкость
    // изменилась на значение, превышающее DELTA_VOLUME_THRESHOLD_VALUE.

    // Определение изменения положения потенциометра
    int16_t deltaPotentiometerPosition = abs((int16_t) pAdcCountsInPercents[POTENTIOMETER_VOLUME_CONTROL] - (int16_t) lastPotentiometerPositionInPercents);
    
    // Если изменение положения потенциометра превышает пороговое значение
    if (deltaPotentiometerPosition > DELTA_VOLUME_THRESHOLD_VALUE)
    {
        // Обновление последнего положения потенциометра
        lastPotentiometerPositionInPercents = pAdcCountsInPercents[POTENTIOMETER_VOLUME_CONTROL];
        
        // Текущим источником управления
        // громкостью звука становится потенциометр
        volumeControlSource = VOLUME_CONTROL_SOURCE_POTENTIOMETER;

        // Сохранение времени последнего
        // изменения положения потенциометра
        lastPotentiometerChangePositionTime = UserTimer_GetCounterTime();

        // Обновление текущей громкости звука
        currentVolumeInPercents = pAdcCountsInPercents[POTENTIOMETER_VOLUME_CONTROL];
        
        // Ограничение максимальной громкости
        if (currentVolumeInPercents > MAX_LIMITED_VOLUME_PERCENT)
        {
            currentVolumeInPercents = MAX_LIMITED_VOLUME_PERCENT;
        }
        
        // Защита от опасной комбинации громкости и пресета
        VolumeProtectionCheck(currentVolumeInPercents);
        
        // Установка громкости звука
        volumeStream.SetVolume(currentVolumeInPercents);
        
        // Расчет громкости звука в рамках протокола AVRCP
        uint8_t avrcpVolume = (currentVolumeInPercents * MAX_VOLUME_AVRCP) / MAX_VOLUME_PERCENT;

        // Обновление текущей громкости звука
        // смартфоне для синхронизации значения
        a2dp_sink.set_volume(avrcpVolume);
        
        #ifdef DEBUG_INFO_VOLUME_CONTROL

            Serial.printf("Громкость, установленная потенциометром: %u%%\r\n", currentVolumeInPercents);

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
            SoundControl_SwitchNextTrack();

            #ifdef DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE

                Serial.println("Кнопка управления звуком - действие на второе нажатие");

            #endif // DEBUG_INFO_BUTTON_SOUND_CONTROL_STATE
        }
        else if (THREE_PRESS == pButtonsPressCount[BUTTON_SOUND_CONTROL])   // Зафиксировано три нажатия на кнопку
        {
            // Переключение на предыдущий трек
            SoundControl_SwitchPreviousTrack();

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

//! \brief Получение указателя на объект BluetoothA2DPSink
//! \return Указатель на объект BluetoothA2DPSink
BluetoothA2DPSink * SoundControl_GetA2DPSinkPointer(void)
{
    return &a2dp_sink;
}

//! \brief Получение указателя на объект I2SStream
//! \return Указатель на объект I2SStream
I2SStream * SoundControl_GetI2SStreamPointer(void)
{
    return &i2s;
}