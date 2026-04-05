#include <AudioTools.h>

#include "AudioNotifications.h"
#include "SoundControl.h"
#include "UserTimer.h"

// #define DEBUG_INFO_AUDIO_NOTIFICATIONS          // Вывод информации о работе модуля аудио уведомлений

// Настройки звукового уведомления
#define NOTIFICATION_DURATION_MS    300         //!< Общая длительность уведомления (мс)
#define NOTIFICATION_AMPLITUDE      3500        //!< Амплитуда сигнала (0-32767)
#define NOTIFICATION_BUFFER_SIZE    1024        //!< Размер буфера для отправки в I2S (количество сэмплов)

#define MS_IN_SECOND_QUANTITY       1000        //!< Количество миллисекунд в одной секунде
#define NOTIFICATION_DURATION_SEC   ((float) NOTIFICATION_DURATION_MS / MS_IN_SECOND_QUANTITY)  //!< Продолжительность уведомления в секундах

//! Общее количество сэмплов для воспроизведения уведомления
#define NOTIFICATION_TOTAL_SAMPLES  ((NOTIFICATION_DURATION_MS * SAMPLE_RATE) / MS_IN_SECOND_QUANTITY)

// Частоты сигналов
#define CONNECT_FREQUENCY           880.0f      //!< Частота сигнала при подключении (Гц)
#define DISCONNECT_FREQUENCY        440.0f      //!< Частота сигнала при отключении (Гц)

// Параметры огибающей (два коротких импульса)
#define FIRST_IMPULSE_START         0.0f        //!< Начало первого импульса (с)
#define FIRST_IMPULSE_END           0.1f        //!< Конец первого импульса (с)
#define SECOND_IMPULSE_START        0.15f       //!< Начало второго импульса (с)
#define SECOND_IMPULSE_END          0.25f       //!< Конец второго импульса (с)

#define IMPULSE_MAX_AMPLITUDE       1.0f        //!< Максимальная амплитуда огибающей

#define BYTES_IN_SAMPLE             2           //!< Количество байт на один сэмпл (16 бит)
#define BYTES_IN_STEREO_SAMPLE      (BYTES_IN_SAMPLE * AUDIO_CHANNELS_QUANTITY)  //!< Размер стерео-сэмпла в байтах

// static const float pi = 3.14159f;               //!< Число Пи

//! \brief Состояние воспроизведения уведомления
typedef enum
{
    NOTIFICATION_NOT_PLAYING = 0,               //!< Уведомление не воспроизводится
    NOTIFICATION_PLAYING                        //!< Уведомление воспроизводится
} NotificationPlaybackState;

//! \brief Структура для хранения состояния модуля
typedef struct
{
    NotificationPlaybackState playbackState;    //!< Состояние воспроизведения
    NotificationType currentType;               //!< Текущий тип уведомления
    uint32_t samplesSent;                       //!< Отправленное количество сэмплов
    float phase;                                //!< Текущая фаза синусоиды
} NotificationPlayer;

static NotificationPlayer player;    //!< Состояние модуля

//! \brief Расчет огибающей импульса
//! \param[in] time - текущее время
//! \param[in] startTime - время начала импульса
//! \param[in] endTime - время завершения импульса
//! \return Значение огибающей (0.0 - 1.0)
static float CalculateImpulseEnvelope(float time, float startTime, float endTime)
{
    // Если текущее время не попадает в интервал импульса
    if ((time < startTime) || (time >= endTime))
    {
        return 0.0f;
    }
    
    // Нормированное время внутри импульса (0.0 - 1.0)
    float normalizedTime = (time - startTime) / (endTime - startTime);
    
    // Примечание. Огибающая имеет колоколообразную форму: sin(pi * time)^2.
    // Возведение в квадрат обеспечивает плавное нарастание и затухание,
    // а также отсутствие резких скачков на границах импульса

    // Расчет огибающей
    float envelope = sinf(pi * normalizedTime);

    // Возведение огибающей в квадрат
    float squareEnvelope = envelope * envelope;
    
    return squareEnvelope;
}

//! \brief Преобразование моно-буфера в стерео (дублирование каналов)
//! \param[in] monoBuffer - входной моно-буфер
//! \param[out] stereoBuffer - выходной стерео-буфер
//! \param[in] samplesQuantity - количество сэмплов в моно-буфере
static void MonoToStereoConvert(const int16_t *monoBuffer, int16_t *stereoBuffer, uint32_t samplesQuantity)
{
    for (uint32_t sampleIndex = 0; sampleIndex < samplesQuantity; sampleIndex++)
    {
        stereoBuffer[sampleIndex * 2] = monoBuffer[sampleIndex];      // Левый канал
        stereoBuffer[sampleIndex * 2 + 1] = monoBuffer[sampleIndex];  // Правый канал
    }
}

//! \brief Генерация звукового сигнала с двумя короткими импульсами
//! \param[out] samplesBuffer - выходной буфер сэмплов
//! \param[in] samplesQuantity - количество сэмплов для генерации
//! \param[in] startSample - номер начального сэмпла в общем потоке
static void GenerateTone(int16_t *samplesBuffer, uint32_t samplesQuantity, uint32_t startSample)
{
    // Выбор частоты сигнала в зависимости от типа уведомления
    float frequency = (player.currentType == NOTIFICATION_CONNECT) ? CONNECT_FREQUENCY : DISCONNECT_FREQUENCY;
    
    for (uint32_t sampleIndex = 0; sampleIndex < samplesQuantity; sampleIndex++)
    {
        // Текущее абсолютное время
        float currentTime = (float) (startSample + sampleIndex) / SAMPLE_RATE;
        
        // Если время вышло за пределы длительности,
        // то ничего не воспроизводится
        if (currentTime >= NOTIFICATION_DURATION_SEC)
        {
            samplesBuffer[sampleIndex] = 0;
            
            continue;
        }
        
        // Расчет огибающей из двух импульсов
        float envelope = 0.0f;
        
        // Расчет огибающей первого импульса
        envelope += CalculateImpulseEnvelope(currentTime, FIRST_IMPULSE_START, FIRST_IMPULSE_END);
        
        // Расчет огибающей второго импульса
        envelope += CalculateImpulseEnvelope(currentTime, SECOND_IMPULSE_START, SECOND_IMPULSE_END);
        
        // Ограничение суммарной огибающей
        if (envelope > IMPULSE_MAX_AMPLITUDE)
        {
            envelope = IMPULSE_MAX_AMPLITUDE;
        }
        
        // Пояснение. Расчет приращения фазы для текущей частоты.
        // deltaPhase = 2 * pi * frequency / NOTIFICATION_SAMPLE_RATE,
        // где deltaPhase - приращение частоты фазы;
        //     frequency - частота сигнала;
        //     NOTIFICATION_SAMPLE_RATE - частота дискретизации.
        // При этом (2 * pi) радиан составляют один полный период синусоиды.

        // Расчет приращения фазы для текущей частоты
        float deltaPhase = PHASE_CYCLE * pi * frequency / SAMPLE_RATE;

        // Текущая фаза синусоиды
        player.phase += deltaPhase;
        
        // Нормализация фазы
        if (player.phase > PHASE_CYCLE * pi)
        {
            player.phase -= PHASE_CYCLE * pi;
        }
        
        // Генерация синусоидального сигнала с огибающей
        float sampleValue = NOTIFICATION_AMPLITUDE * envelope * sinf(player.phase);
        
        // Ограничение громкости для 16-ти битного сэмпла
        if (sampleValue > MAX_VOLUME_SAMPLE)
        {
            sampleValue = MAX_VOLUME_SAMPLE;
        }
        else if (sampleValue < MIN_VOLUME_SAMPLE)
        {
            sampleValue = MIN_VOLUME_SAMPLE;
        }
        
        // Сохранение полученного значения в буфер
        samplesBuffer[sampleIndex] = (int16_t) sampleValue;
    }
}

//! \brief Инициализация звуковых уведомлений
void AudioNotifications_Init(void)
{
    player.playbackState = NOTIFICATION_NOT_PLAYING;    // Установка состояния - уведомление не воспроизводится
    player.currentType = NOTIFICATION_CONNECT;          // Текущий тип уведомление - подключение
    player.samplesSent = 0;                             // Отправленное количество сэмплов
    player.phase = 0;                                   // Текущая фаза синусоиды
    
    #ifdef DEBUG_INFO_AUDIO_NOTIFICATIONS

        Serial.println("Модуль звуковых уведомлений инициализирован");

    #endif // DEBUG_INFO_AUDIO_NOTIFICATIONS
}

//! \brief Воспроизведение звукового уведомления
//! \param[in] notificationType - тип уведомления
void AudioNotifications_Play(NotificationType notificationType)
{
    // Выход, если уведомление уже воспроизводится
    if (NOTIFICATION_PLAYING == player.playbackState)
    {
        return;
    }
    
    player.currentType = notificationType;          // Установка типа уведомления
    player.playbackState = NOTIFICATION_PLAYING;    // Установка состояния - уведомление воспроизводится
    player.samplesSent = 0;                         // Количество отправленных сэмплов
    player.phase = 0;                               // Текущая фаза синусоиды
    
    #ifdef DEBUG_INFO_AUDIO_NOTIFICATIONS

        float frequency = (notificationType == NOTIFICATION_CONNECT) ? CONNECT_FREQUENCY : DISCONNECT_FREQUENCY;

        Serial.printf("Начато воспроизведение (%s, %.0f Гц)\r\n",
                      notificationType == NOTIFICATION_CONNECT ? "подключение" : "отключение", frequency);

    #endif // DEBUG_INFO_AUDIO_NOTIFICATIONS
}

//! \brief Обновление состояния воспроизведения
void AudioNotifications_Update(void)
{
    // Выход, если воспроизведение не активно
    if (NOTIFICATION_PLAYING != player.playbackState)
    {
        return;
    }
    
    // Определение количества сэмплов, которое осталось отправить
    uint32_t remainingSamples = NOTIFICATION_TOTAL_SAMPLES - player.samplesSent;
    
    // Определение размера буфера данных для генерации
    uint32_t samplesBufferSize = NOTIFICATION_BUFFER_SIZE;
    
    // Если осталось передать меньше данных,
    // чем размер буфера, то происходит уменьшение буфера
    if (samplesBufferSize > remainingSamples)
    {
        samplesBufferSize = remainingSamples;
    }
    
    // Буфер для генерации сэмплов (моно)
    int16_t monoBuffer[NOTIFICATION_BUFFER_SIZE];
    
    // Генерация звука (сэмплов)
    GenerateTone(monoBuffer, samplesBufferSize, player.samplesSent);
    
    // Буфер для стерео звука
    int16_t stereoBuffer[NOTIFICATION_BUFFER_SIZE * AUDIO_CHANNELS_QUANTITY];

    // Преобразование звука из моно в стерео
    MonoToStereoConvert(monoBuffer, stereoBuffer, samplesBufferSize);
    
    // Получение адреса объекта I2SStream
    I2SStream *i2s = SoundControl_GetI2SStreamPointer();

    // Если указатель существует
    if (NULL != i2s)
    {
        // Вывод звука через I2S
        i2s->write((uint8_t *) stereoBuffer, samplesBufferSize * BYTES_IN_STEREO_SAMPLE);
    }
    
    // Инкремент количества отправленных сэмплов
    player.samplesSent += samplesBufferSize;
    
    // Если все сэмплы отправлены
    if (player.samplesSent >= NOTIFICATION_TOTAL_SAMPLES)
    {
        // Установка состояния - уведомление не воспроизводится
        player.playbackState = NOTIFICATION_NOT_PLAYING;
        
        #ifdef DEBUG_INFO_AUDIO_NOTIFICATIONS

            Serial.println("Воспроизведение завершено");

        #endif // DEBUG_INFO_AUDIO_NOTIFICATIONS
    }
}