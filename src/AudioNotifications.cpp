#include <AudioTools.h>

#include "AudioNotifications.h"
#include "SoundControl.h"
#include "UserTimer.h"

// #define DEBUG_INFO_AUDIO_NOTIFICATIONS

#define NOTIFICATION_SAMPLE_RATE   44100   //!< Частота дискретизации
#define NOTIFICATION_DURATION_MS   300     //!< Длительность уведомления (мс)
#define NOTIFICATION_AMPLITUDE     3500    //!< Амплитуда
#define NOTIFICATION_BUFFER_SIZE   1024    //!< Размер буфера (сэмплов)

// Частоты для разных событий
#define CONNECT_FREQ     880.0f   // Подключение: 880 Гц
#define DISCONNECT_FREQ  440.0f   // Отключение: 440 Гц

// Структура для хранения состояния воспроизведения
typedef struct
{
    bool isPlaying;
    bool isInitialized;
    NotificationType currentType;
    uint32_t samplesRemaining;
    uint32_t samplesSent;
    float phase;
} NotificationPlayer;

static NotificationPlayer player;

// два коротких импульса
static void generateTone(int16_t* buffer, uint32_t samplesCount, uint32_t startSample)
{
    float duration = NOTIFICATION_DURATION_MS / 1000.0f;
    float freq = (player.currentType == NOTIFICATION_CONNECT) ? CONNECT_FREQ : DISCONNECT_FREQ;
    
    for (uint32_t i = 0; i < samplesCount; i++)
    {
        float t = (float)(startSample + i) / NOTIFICATION_SAMPLE_RATE;
        if (t >= duration) { buffer[i] = 0; continue; }
        
        // Два импульса: 0-0.1с и 0.15-0.25с
        float envelope = 0;
        if (t < 0.1f) envelope = sinf(3.14159f * t / 0.1f);           // первый импульс
        else if (t > 0.15f && t < 0.25f) envelope = sinf(3.14159f * (t - 0.15f) / 0.1f);  // второй импульс
        
        // Плавное нарастание и затухание импульсов
        envelope = envelope * envelope;
        
        float deltaPhase = 2.0f * 3.14159f * freq / NOTIFICATION_SAMPLE_RATE;
        player.phase += deltaPhase;
        if (player.phase > 2.0f * 3.14159f) player.phase -= 2.0f * 3.14159f;
        
        float value = NOTIFICATION_AMPLITUDE * envelope * sinf(player.phase);
        if (value > 32767) value = 32767;
        if (value < -32768) value = -32768;
        
        buffer[i] = (int16_t)value;
    }
}

void AudioNotifications_Init(void)
{
    player.isPlaying = false;
    player.isInitialized = true;
    player.currentType = NOTIFICATION_CONNECT;
    player.samplesRemaining = 0;
    player.samplesSent = 0;
    player.phase = 0;
    
    #ifdef DEBUG_INFO_AUDIO_NOTIFICATIONS
        Serial.println("Модуль звуковых уведомлений инициализирован (вариант 1)");
    #endif
}

void AudioNotifications_Play(NotificationType type)
{
    if (!player.isInitialized || player.isPlaying) return;
    
    uint32_t totalSamples = (NOTIFICATION_DURATION_MS * NOTIFICATION_SAMPLE_RATE) / 1000;
    
    player.currentType = type;
    player.isPlaying = true;
    player.samplesRemaining = totalSamples;
    player.samplesSent = 0;
    player.phase = 0;
    
    #ifdef DEBUG_INFO_AUDIO_NOTIFICATIONS
        float freq = (type == NOTIFICATION_CONNECT) ? CONNECT_FREQ : DISCONNECT_FREQ;
        Serial.printf("Начато воспроизведение (%s, %.0f Гц)\r\n",
                     type == NOTIFICATION_CONNECT ? "подключение" : "отключение", freq);
    #endif
}

void AudioNotifications_Update(void)
{
    if (!player.isInitialized || !player.isPlaying) return;
    
    uint32_t samplesToGenerate = NOTIFICATION_BUFFER_SIZE;
    if (samplesToGenerate > player.samplesRemaining)
    {
        samplesToGenerate = player.samplesRemaining;
    }
    
    if (samplesToGenerate > 0)
    {
        int16_t buffer[NOTIFICATION_BUFFER_SIZE];
        generateTone(buffer, samplesToGenerate, player.samplesSent);
        
        int16_t stereoBuffer[NOTIFICATION_BUFFER_SIZE * 2];
        for (uint32_t i = 0; i < samplesToGenerate; i++)
        {
            stereoBuffer[i * 2] = buffer[i];
            stereoBuffer[i * 2 + 1] = buffer[i];
        }
        
        I2SStream *i2s = SoundControl_GetI2SStreamPointer();
        if (i2s != NULL)
        {
            i2s->write((uint8_t*)stereoBuffer, samplesToGenerate * 4);
        }
        
        player.samplesSent += samplesToGenerate;
        player.samplesRemaining -= samplesToGenerate;
    }
    
    if (player.samplesRemaining == 0)
    {
        player.isPlaying = false;
        #ifdef DEBUG_INFO_AUDIO_NOTIFICATIONS
            Serial.println("Воспроизведение завершено");
        #endif
    }
}

bool AudioNotifications_IsPlaying(void)
{
    return player.isPlaying;
}