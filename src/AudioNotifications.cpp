#include <AudioTools.h>

#include "AudioNotifications.h"
#include "SoundControl.h"
#include "UserTimer.h"

// #define DEBUG_INFO_AUDIO_NOTIFICATIONS      // Вывод информации о работе модуля аудио уведомлений

// Настройки звукового уведомления
#define NOTIFICATION_SAMPLE_RATE    44100   //!< Частота дискретизации (Гц)
#define NOTIFICATION_DURATION_MS    300     //!< Общая длительность уведомления (мс)
#define NOTIFICATION_AMPLITUDE      3500    //!< Амплитуда сигнала (0-32767)
#define NOTIFICATION_BUFFER_SIZE    1024    //!< Размер буфера для отправки в I2S (сэмплов)

// Частоты сигналов
#define CONNECT_FREQUENCY           880.0f  //!< Частота сигнала при подключении (Гц)
#define DISCONNECT_FREQUENCY        440.0f  //!< Частота сигнала при отключении (Гц)

// Параметры огибающей (два коротких импульса)
#define FIRST_IMPULSE_START         0.0f    //!< Начало первого импульса (с)
#define FIRST_IMPULSE_END           0.1f    //!< Конец первого импульса (с)
#define SECOND_IMPULSE_START        0.15f   //!< Начало второго импульса (с)
#define SECOND_IMPULSE_END          0.25f   //!< Конец второго импульса (с)

#define IMPULSE_MAX_AMPLITUDE       1.0f    //!< Максимальная амплитуда огибающей
#define PHASE_CYCLE                 2.0f    //!< Полный цикл фазы (2 * pi радиан)

#define MS_IN_SECOND_QUANTITY       1000     //!< Количество миллисекунд в одной секунде

#define BYTES_IN_SAMPLE             2       //!< Количество байт на один сэмпл (16 бит)
#define BYTES_IN_STEREO_SAMPLE      (BYTES_IN_SAMPLE * AUDIO_CHANNELS_QUANTITY)  //!< Размер стерео-сэмпла в байтах

static const float pi = 3.14159f;            //!< Число Пи

//! \brief Состояние воспроизведения уведомления
typedef enum
{
    NOTIFICATION_PLAYING = 0,           //!< Уведомление воспроизводится
    NOTIFICATION_NOT_PLAYING            //!< Уведомление не воспроизводится
} NotificationPlaybackState;

//! \brief Состояние инициализации модуля
typedef enum
{
    NOTIFICATION_NOT_INITIALIZED = 0,   //!< Модуль не инициализирован
    NOTIFICATION_INITIALIZED            //!< Модуль инициализирован
} NotificationInitState;

//! \brief Структура для хранения состояния модуля
typedef struct
{
    NotificationPlaybackState playbackState;    //!< Состояние воспроизведения
    NotificationInitState initState;            //!< Состояние инициализации модуля
    NotificationType currentType;               //!< Текущий тип уведомления
    uint32_t samplesRemaining;                  //!< Оставшееся количество сэмплов
    uint32_t samplesSent;                       //!< Отправленное количество сэмплов
    float phase;                                //!< Текущая фаза синусоиды
} NotificationPlayer;

static NotificationPlayer player;    //!< Состояние модуля

//! \brief Расчет огибающей импульса
//! \param[in] time - текущее время
//! \param[in] startTime - время начала импульса
//! \param[in] endTime - время завершения импульса
//! \return Значение огибающей (0.0 - 1.0)
static float GetImpulseEnvelope(float time, float startTime, float endTime)
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

//! \brief Генерация звукового сигнала с двумя короткими импульсами
//! \param[out] samplesBuffer - выходной буфер сэмплов
//! \param[in] samplesQuantity - количество сэмплов для генерации
//! \param[in] startSample - номер начального сэмпла в общем потоке
static void GenerateTone(int16_t* samplesBuffer, uint32_t samplesQuantity, uint32_t startSample)
{
    // Продолжительность уведомления [с]
    float notificationDuration = (float) NOTIFICATION_DURATION_MS / MS_IN_SECOND_QUANTITY;
    
    // Выбор частоты сигнала в зависимости от типа уведомления
    float frequency = (player.currentType == NOTIFICATION_CONNECT) ? CONNECT_FREQUENCY : DISCONNECT_FREQUENCY;
    
    for (uint32_t sampleIndex = 0; sampleIndex < samplesQuantity; sampleIndex++)
    {
        // Текущее абсолютное время
        float currentTime = (float) (startSample + sampleIndex) / NOTIFICATION_SAMPLE_RATE;
        
        // Если время вышло за пределы длительности,
        // то ничего не воспроизводится
        if (currentTime >= notificationDuration)
        {
            samplesBuffer[sampleIndex] = 0;
            
            continue;
        }
        
        // Расчет огибающей из двух импульсов
        float envelope = 0.0f;
        
        // Расчет огибающей первого импульса
        envelope += GetImpulseEnvelope(currentTime, FIRST_IMPULSE_START, FIRST_IMPULSE_END);
        
        // Расчет огибающей второго импульса
        envelope += GetImpulseEnvelope(currentTime, SECOND_IMPULSE_START, SECOND_IMPULSE_END);
        
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
        float deltaPhase = PHASE_CYCLE * pi * frequency / NOTIFICATION_SAMPLE_RATE;

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

//! \brief Инициализация модуля звуковых уведомлений
void AudioNotifications_Init(void)
{
    player.playbackState = NOTIFICATION_NOT_PLAYING;    // Установка состояния - уведомление не воспроизводится
    player.initState = NOTIFICATION_INITIALIZED;        // Состояние инициализации модуля - модуль инициализирован
    player.currentType = NOTIFICATION_CONNECT;          // Текущий тип уведомление - подключение
    player.samplesRemaining = 0;                        // Оставшееся количество сэмплов
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
    // Выход из функции, если модуль не инициализирован
    // или уже воспроизводится уведомление
    if ((NOTIFICATION_INITIALIZED != player.initState) ||
        (NOTIFICATION_NOT_PLAYING != player.playbackState))
    {
        return;
    }
    
    // Расчет общего количества сэмплов
    uint32_t totalSamples = (NOTIFICATION_DURATION_MS * NOTIFICATION_SAMPLE_RATE) / MS_IN_SECOND_QUANTITY;
    
    player.currentType = notificationType;          // Установка типа уведомления
    player.playbackState = NOTIFICATION_PLAYING;    // Установка состояния - уведомление воспроизводится
    player.samplesRemaining = totalSamples;         // Общее количество сэмплов
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
    // Выход из функции, если модуль не инициализирован
    // или воспроизведение не активно
    if ((NOTIFICATION_INITIALIZED != player.initState) ||
        (NOTIFICATION_NOT_PLAYING == player.playbackState))
    {
        return;
    }
    
    // Размер буфера для генерации сэмплов
    uint32_t sampleBufferSize = NOTIFICATION_BUFFER_SIZE;

    // Ограничение количества оставшихся сэмплов
    if (sampleBufferSize > player.samplesRemaining)
    {
        sampleBufferSize = player.samplesRemaining;
    }
    
    // Если остались сэмплы для генерации
    if (sampleBufferSize > 0)
    {
        // Буфер для генерации сэмплов
        int16_t sampleBuffer[NOTIFICATION_BUFFER_SIZE];
        
        // Генерация звука (сэмплов)
        GenerateTone(sampleBuffer, sampleBufferSize, player.samplesSent);
        
        // Буфер для стерео звука
        int16_t stereoBuffer[NOTIFICATION_BUFFER_SIZE * AUDIO_CHANNELS_QUANTITY];

        // Преобразование звука из моно в стерео (дублирование каналов)
        for (uint32_t sampleIndex = 0; sampleIndex < sampleBufferSize; sampleIndex++)
        {
            stereoBuffer[sampleIndex * 2] = sampleBuffer[sampleIndex];      // Левый канал
            stereoBuffer[sampleIndex * 2 + 1] = sampleBuffer[sampleIndex];  // Правый канал
        }
        
        // Получение адреса объекта класса I2SStream
        I2SStream *i2s = SoundControl_GetI2SStreamPointer();

        // Если есть указатель
        if (NULL != i2s)
        {
            // Вывод звука через I2S
            i2s->write((uint8_t *) stereoBuffer, sampleBufferSize * BYTES_IN_STEREO_SAMPLE);
        }
        
        // Инкремент количества отправленных сэмплов
        player.samplesSent += sampleBufferSize;

        // Декремент количества оставшихся сэмплов
        player.samplesRemaining -= sampleBufferSize;
    }
    
    // Если все сэмплы отправлены
    if (0 == player.samplesRemaining)
    {
        // Установка состояния - уведомление не воспроизводится
        player.playbackState = NOTIFICATION_NOT_PLAYING;
        
        #ifdef DEBUG_INFO_AUDIO_NOTIFICATIONS

            Serial.println("Воспроизведение завершено");

        #endif // DEBUG_INFO_AUDIO_NOTIFICATIONS
    }
}