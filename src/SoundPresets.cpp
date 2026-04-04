#include "SoundPresets.h"
#include "ButtonsDriver.h"
#include "UserTimer.h"
#include <math.h>

#include <Arduino.h>

// Раскомментируй для отладки
#define DEBUG_INFO_SOUND_PRESETS

// Частота дискретизации
#define SAMPLE_RATE                 44100       //!< Частота дискретизации (Гц)

// Частоты среза для трёх полос эквалайзера
#define LOW_CROSSOVER_FREQ          250         //!< Частота среза низких/средних частот (Гц)
#define HIGH_CROSSOVER_FREQ         4000        //!< Частота среза средних/высоких частот (Гц)

// Коэффициенты фильтров (рассчитываются один раз при инициализации)
static float lowAlpha;      //!< Коэффициент ФНЧ для низких частот
static float highAlpha;     //!< Коэффициент ФВЧ для высоких частот

// Состояния фильтров для левого и правого каналов
typedef struct
{
    float lowPrev;      //!< Предыдущее значение низких частот
    float highPrev;     //!< Предыдущее значение высоких частот
} FilterState;

static FilterState leftChannel;     //!< Состояние фильтров левого канала
static FilterState rightChannel;    //!< Состояние фильтров правого канала

// Текущие коэффициенты усиления
static float lowGain = 1.0f;        //!< Коэффициент усиления низких частот
static float midGain = 1.0f;        //!< Коэффициент усиления средних частот
static float highGain = 1.0f;       //!< Коэффициент усиления высоких частот
static EqPreset currentPreset = EQ_PRESET_NORMAL;     //!< Текущий пресет

// Счётчик для отладки (чтобы не заспамить Serial)
static uint32_t lastDebugTime = 0;
#define DEBUG_PRINT_INTERVAL_TICKS  10000   // Печатать раз в секунду (10000 тиков * 100 мкс = 1 сек)

//! \brief Коэффициенты усиления для каждого пресета в децибелах
static const int8_t presetGainsDb[EQ_PRESETS_QUANTITY][3] = 
{
    // Оригинальные
    {  0,  0,  0 },   // NORMAL - плоская характеристика
    { 12, -3, -6 },   // BASS - сильное усиление низких
    { -6, -3, 12 },   // TREBLE - сильное усиление высоких
    { -6,  8, -3 },   // VOCAL - усиление средних (голос)
    {  6,  4,  6 },   // ROCK - подъём всех частот
    {  8, -2,  8 },   // ELECTRO - подъём низких и высоких (V-образная)
};

// Названия пресетов для отладки
static const char* presetNames[EQ_PRESETS_QUANTITY] = 
{
    "NORMAL",
    "BASS",
    "TREBLE",
    "VOCAL",
    "ROCK",
    "ELECTRO",
};

//! \brief Преобразование децибел в линейный коэффициент (экспоненциальное)
//! \param[in] db - значение в децибелах
//! \return Линейный коэффициент
static float DbToLinear(int8_t db)
{
    // Формула: linear = 10^(db/20)
    return powf(10.0f, db / 20.0f);
}

// //! \brief Преобразование децибел в линейный коэффициент
// //! \param[in] db - значение в децибелах
// //! \return Линейный коэффициент
// static float DbToLinear(int8_t db)
// {
//     // Более точная формула: linear = 10^(db/20)
//     if (db == 0)
//     {
//         return 1.0f;
//     }
//     else if (db > 0)
//     {
//         return 1.0f + (db / 10.0f);
//     }
//     else
//     {
//         return 1.0f - (abs(db) / 15.0f);
//     }
// }

//! \brief Обновление коэффициентов усиления в соответствии с текущим пресетом
static void SoundPresets_UpdateGains(void)
{
    lowGain = DbToLinear(presetGainsDb[currentPreset][0]);
    midGain = DbToLinear(presetGainsDb[currentPreset][1]);
    highGain = DbToLinear(presetGainsDb[currentPreset][2]);
    
    #ifdef DEBUG_INFO_SOUND_PRESETS
        Serial.printf("[EQ] Пресет: %s | Low: %d dB (%.2fx), Mid: %d dB (%.2fx), High: %d dB (%.2fx)\r\n",
                      presetNames[currentPreset],
                      presetGainsDb[currentPreset][0], lowGain,
                      presetGainsDb[currentPreset][1], midGain,
                      presetGainsDb[currentPreset][2], highGain);
    #endif
}

//! \brief Фильтр низких частот (Low-pass filter) первого порядка
//! \param[in] input - входное значение
//! \param[in] alpha - коэффициент фильтра
//! \param[in,out] prev - предыдущее выходное значение
//! \return Отфильтрованное значение
static float LowPassFilter(float input, float alpha, float *prev)
{
    float output = alpha * input + (1.0f - alpha) * (*prev);
    *prev = output;
    return output;
}

//! \brief Фильтр высоких частот (High-pass filter) первого порядка
//! \param[in] input - входное значение
//! \param[in] alpha - коэффициент фильтра
//! \param[in,out] prev - предыдущее выходное значение
//! \return Отфильтрованное значение
static float HighPassFilter(float input, float alpha, float *prev)
{
    // Формула ФВЧ: y[n] = alpha * (x[n] - x[n-1] + y[n-1])
    float output = alpha * (input - (*prev) + output);
    *prev = input;
    return output;
}

//! \brief Разделение сигнала на три полосы и применение усиления
//! \param[in] sample - входной сэмпл
//! \param[in,out] state - состояние фильтров для канала
//! \return Обработанный сэмпл
static int16_t ProcessChannel(int16_t sample, FilterState *state)
{
    float input = (float) sample;
    
    // Выделение низких частот (Low-pass)
    float lowBand = LowPassFilter(input, lowAlpha, &state->lowPrev);
    
    // Выделение высоких частот (High-pass)
    float highBand = HighPassFilter(input, highAlpha, &state->highPrev);
    
    // Выделение средних частот (исходный сигнал минус низкие и высокие)
    float midBand = input - lowBand - highBand;
    
    // Применение усиления к каждой полосе
    float processedLow = lowBand * lowGain;
    float processedMid = midBand * midGain;
    float processedHigh = highBand * highGain;
    
    // Смешивание обработанных полос
    float output = processedLow + processedMid + processedHigh;
    
    // Ограничение диапазона
    if (output > 32767.0f)
    {
        output = 32767.0f;
    }
    else if (output < -32768.0f)
    {
        output = -32768.0f;
    }
    
    return (int16_t) output;
}

//! \brief Инициализация модуля звуковых пресетов
void SoundPresets_Init(void)
{
    // Расчёт коэффициентов фильтров
    // alpha = 1 - exp(-2 * pi * Fc / Fs)
    float omegaLow = 2.0f * 3.14159265f * LOW_CROSSOVER_FREQ / SAMPLE_RATE;
    float omegaHigh = 2.0f * 3.14159265f * HIGH_CROSSOVER_FREQ / SAMPLE_RATE;
    
    lowAlpha = 1.0f - expf(-omegaLow);
    highAlpha = 1.0f - expf(-omegaHigh);
    
    #ifdef DEBUG_INFO_SOUND_PRESETS
        Serial.printf("[EQ] Инициализация: lowAlpha=%.4f, highAlpha=%.4f\r\n", lowAlpha, highAlpha);
    #endif
    
    // Обнуление состояния фильтров
    leftChannel.lowPrev = 0.0f;
    leftChannel.highPrev = 0.0f;
    
    rightChannel.lowPrev = 0.0f;
    rightChannel.highPrev = 0.0f;
    
    // Установка начального пресета
    SoundPresets_UpdateGains();
}

//! \brief Применение эквалайзера к аудиосэмплу
//! \param[in] sample - входной сэмпл
//! \param[in] channel - номер канала (0=левый, 1=правый)
//! \return Обработанный сэмпл
int16_t SoundPresets_ProcessSample(int16_t sample, uint8_t channel)
{
    if (channel == 0)
    {
        return ProcessChannel(sample, &leftChannel);
    }
    else
    {
        return ProcessChannel(sample, &rightChannel);
    }
}

//! \brief Установка пресета эквалайзера
//! \param[in] preset - пресет из перечисления EqPreset
void SoundPresets_SetPreset(EqPreset preset)
{
    if (preset >= EQ_PRESETS_QUANTITY)
    {
        return;
    }
    
    currentPreset = preset;
    SoundPresets_UpdateGains();
}

//! \brief Получение текущего пресета эквалайзера
//! \return Текущий пресет
EqPreset SoundPresets_GetCurrentPreset(void)
{
    return currentPreset;
}

//! \brief Переключение на следующий пресет
void SoundPresets_NextPreset(void)
{
    EqPreset newPreset = (EqPreset) (currentPreset + 1);
    
    if (newPreset >= EQ_PRESETS_QUANTITY)
    {
        newPreset = EQ_PRESET_NORMAL;
    }
    
    SoundPresets_SetPreset(newPreset);
}

//! \brief Переключение на предыдущий пресет
void SoundPresets_PreviousPreset(void)
{
    if (currentPreset == EQ_PRESET_NORMAL)
    {
        SoundPresets_SetPreset((EqPreset) (EQ_PRESETS_QUANTITY - 1));
    }
    else
    {
        SoundPresets_SetPreset((EqPreset) (currentPreset - 1));
    }
}

//! \brief Сброс к пресету Normal
void SoundPresets_ResetToNormal(void)
{
    SoundPresets_SetPreset(EQ_PRESET_NORMAL);
}

//! \brief Управление пресетами эквалайзера (обработка нажатий кнопки)
void SoundPresets_Control(void)
{
    // Получение адреса массива с количеством устойчивых нажатий на кнопки
    uint8_t *pButtonsPressCount = ButtonsDriver_GetButtonsPressCountPointer();
    
    // Получение адреса массива со статусами завершения серий нажатий 
    ButtonPressSeriesStatus *pButtonSeriesStatus = ButtonsDriver_GetButtonsPressSeriesStatusPointer();
    
    // Если завершена серия нажатий на кнопку управления пресетами
    if (BUTTON_PRESS_SERIES_FINISHED == pButtonSeriesStatus[BUTTON_SOUND_PRESET_CONTROL])
    {
        #ifdef DEBUG_INFO_SOUND_PRESETS
            Serial.printf("[EQ] Нажатий: %d\r\n", pButtonsPressCount[BUTTON_SOUND_PRESET_CONTROL]);
        #endif
        
        if (ONE_PRESS == pButtonsPressCount[BUTTON_SOUND_PRESET_CONTROL])
        {
            // Одно нажатие - следующий пресет
            SoundPresets_NextPreset();
        }
        else if (TWO_PRESS == pButtonsPressCount[BUTTON_SOUND_PRESET_CONTROL])
        {
            // Два нажатия - предыдущий пресет
            SoundPresets_PreviousPreset();
        }
        else if (THREE_PRESS == pButtonsPressCount[BUTTON_SOUND_PRESET_CONTROL])
        {
            // Три нажатия - сброс к Normal
            SoundPresets_ResetToNormal();
        }
        
        // Сброс статуса завершения серии нажатий
        pButtonSeriesStatus[BUTTON_SOUND_PRESET_CONTROL] = BUTTON_PRESS_SERIES_NONE;
        
        // Сброс количества нажатий
        pButtonsPressCount[BUTTON_SOUND_PRESET_CONTROL] = 0;
    }
}