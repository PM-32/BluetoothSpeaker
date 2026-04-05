#include <math.h>
#include <stdbool.h>

#include <Arduino.h>
#include <Preferences.h>

#include "ButtonsDriver.h"
#include "SoundControl.h"
#include "SoundPresets.h"
#include "UserTimer.h"

// #define DEBUG_INFO_SOUND_PRESETS                    // Вывод информации о текущем пресете
// #define DEBUG_INFO_BUTTON_SOUND_PRESETS             // Вывод информации о нажатии на кнопку управления пресетами
// #define DEBUG_INFO_SOUND_PRESETS_FLASH              // Вывод информации о записи/чтении пресета с flash

// Частоты среза для трех полос эквалайзера
#define LOW_CROSSOVER_FREQUENCY     250             //!< Частота среза низких/средних частот (Гц)
#define HIGH_CROSSOVER_FREQUENCY    4000            //!< Частота среза средних/высоких частот (Гц)

// Математические константы
#define DECIBEL_TO_LINEAR_BASE      10.0f           //!< Основание для перевода децибел в линейный коэффициент
#define DECIBEL_DIVISOR             20.0f           //!< Делитель в формуле перевода децибел (20 dB на декаду)
#define PHASE_CYCLE                 2.0f            //!< Полный цикл фазы (2 * pi радиан)
#define UNITY_GAIN                  1.0f            //!< Единичное усиление (0 dB)
#define ALPHA_COEFF_ONE             1.0f            //!< Единица для расчета альфа коэффициента фильтра

// Настройки сохранения пресета во flash-памяти
#define STORAGE_NAMESPACE           "EqPresets"    //!< Пространство имен для хранения настроек
#define STORAGE_KEY_PRESET          "Preset"       //!< Ключ для хранения текущего пресета

//! \brief Состояния фильтров для левого и правого каналов
typedef struct
{
    float lowFrequencyPrevious;     //!< Предыдущее значение низких частот
    float highFrequencyPrevious;    //!< Предыдущее значение высоких частот
} FilterState;

typedef enum
{
    LOW_FREQUENCIES = 0,            //!< Низкие частоты
    MID_FREQUENCIES,                //!< Средние частоты
    HIGH_FREQUENCIES,               //!< Высокие частоты
    EQUALIZER_BANDS_QUANTITY        //!< Количество полос эквалайзера
} EqualizerBands;

// Коэффициенты фильтров
static float lowAlphaCoeff;         //!< Коэффициент ФНЧ для низких частот
static float highAlphaCoeff;        //!< Коэффициент ФВЧ для высоких частот

static FilterState leftChannel;     //!< Состояние фильтров левого канала
static FilterState rightChannel;    //!< Состояние фильтров правого канала

//!< Коэффициенты усиления (низкие, средние, высокие частоты)
static float frequenciesGainCoeff[EQUALIZER_BANDS_QUANTITY] = { UNITY_GAIN, UNITY_GAIN, UNITY_GAIN };

static EqualizerPreset currentPreset = EQUALIZER_PRESET_NORMAL; //!< Текущий пресет эквалайзера

//! \brief Коэффициенты усиления пресетов в децибелах
static const int8_t presetGainsDb[EQUALIZER_PRESETS_QUANTITY][EQUALIZER_BANDS_QUANTITY] = 
{
    {  0,  0,  0 },   // NORMAL  - плоская характеристика
    { 12, -3, -6 },   // BASS    - сильное усиление низких частот
    { -6, -3, 12 },   // TREBLE  - сильное усиление высоких частот
    { -6,  8, -3 },   // VOCAL   - усиление средних частот (вокал)
    {  6,  4,  6 },   // ROCK    - усиление всех частот
    {  8, -2,  8 },   // ELECTRO - усиление низких и высоких частот
};

// Названия пресетов для отладки
static const char *presetNames[EQUALIZER_PRESETS_QUANTITY] = { "NORMAL", "BASS", "TREBLE", \
                                                               "VOCAL", "ROCK", "ELECTRO" };

//! \brief Запись текущего пресета во flash-память
static void WriteSoundPresetToFlash(void)
{
    // Создание объекта класса Preferences
    Preferences preferences;
    
    // Открытие пространства имен в режиме чтения/записи
    preferences.begin(STORAGE_NAMESPACE, false);
    
    // Сохранение текущего пресета
    preferences.putUChar(STORAGE_KEY_PRESET, (uint8_t) currentPreset);
    
    // Закрытие пространства имен
    preferences.end();
    
    #ifdef DEBUG_INFO_SOUND_PRESETS_FLASH

        Serial.printf("Пресет %s сохранен во flash-память\r\n", presetNames[currentPreset]);

    #endif // DEBUG_INFO_SOUND_PRESETS_FLASH
}

//! \brief Чтение сохраненного пресета из flash-памяти
//! \return Пресет из flash-памяти
static EqualizerPreset ReadSoundPresetFromFlash(void)
{
    // Создание объекта класса Preferences
    Preferences preferences;

    // Номер считанного с flash пресета
    uint8_t savedPreset;
    
    // Открытие пространства имен в режиме только чтения
    preferences.begin(STORAGE_NAMESPACE, true);
    
    // Чтение сохраненного пресета
    // (по умолчанию NORMAL, если ключа нет)
    savedPreset = preferences.getUChar(STORAGE_KEY_PRESET, EQUALIZER_PRESET_NORMAL);
    
    // Закрытие пространства имен
    preferences.end();
    
    // Проверка допустимости считанного значения
    if (savedPreset >= EQUALIZER_PRESETS_QUANTITY)
    {
        savedPreset = EQUALIZER_PRESET_NORMAL;
        
        #ifdef DEBUG_INFO_SOUND_PRESETS_FLASH

            Serial.printf("Загружен невалидный пресет %u, установлен NORMAL\r\n", savedPreset);

        #endif // DEBUG_INFO_SOUND_PRESETS_FLASH
    }
    
    #ifdef DEBUG_INFO_SOUND_PRESETS_FLASH

        Serial.printf("Загружен пресет из flash: %s\r\n", presetNames[savedPreset]);

    #endif // DEBUG_INFO_SOUND_PRESETS_FLASH
    
    return (EqualizerPreset) savedPreset;
}

//! \brief Экспоненциальное преобразование децибел в линейный коэффициент
//! \param[in] dB - значение децибел
//! \return Линейный коэффициент
static float ConvertDbToLinearCoefficient(int8_t dB)
{
    // Пояснение. Формула преобразования: linear = 10^(dB/20),
    // где 10 - основание десятичного логарифма
    //     20 - коэффициент для амплитуды (децибелы для 
    //          амплитуды считаются как 20 * log10(A/A0))
    // Примеры: 0 dB → 1.0 (единичное усиление)
    //          6 dB → 2.0 (усиление в 2 раза)
    //         -6 dB → 0.5 (ослабление в 2 раза)

    return powf(DECIBEL_TO_LINEAR_BASE, dB / DECIBEL_DIVISOR);
}

//! \brief Обновление коэффициентов усиления в соответствии с текущим пресетом
static void SoundPresets_UpdateGains(void)
{
    // Экспоненциальное преобразование децибел в
    // линейный коэффициент для всех полос эквалайзера
    for (uint8_t equalizerBandIndex = 0; equalizerBandIndex < EQUALIZER_BANDS_QUANTITY; equalizerBandIndex++)
    {
        frequenciesGainCoeff[equalizerBandIndex] = ConvertDbToLinearCoefficient(presetGainsDb[currentPreset][equalizerBandIndex]);
    }

    #ifdef DEBUG_INFO_SOUND_PRESETS

        Serial.printf("Текущий пресет: %s\r\n", presetNames[currentPreset]);

    #endif  // DEBUG_INFO_SOUND_PRESETS
}

//! \brief Фильтр низких частот первого порядка
//! \param[in] inputValue - входное значение фильтра
//! \param[in] alphaCoeff - коэффициент фильтра (0 < alpha < 1)
//! \param[in, out] previousValue - предыдущее выходное значение фильтра
//! \return Отфильтрованное значение
static float LowPassFilterFirstOrder(float inputValue, float alphaCoeff, float *previousValue)
{
    // Пояснение. Формула ФНЧ первого порядка:
    // y[n] = alpha * x[n] + (1 - alpha) * y[n-1],
    // где y[n] - текущее выходное значение,
    //     y[n-1] - предыдущее выходное значение,
    //     x[n] - текущее входное значение,
    //     alpha - коэффициент фильтра (чем меньше, тем сильнее сглаживание)

    float outputValue = alphaCoeff * inputValue + (UNITY_GAIN - alphaCoeff) * (*previousValue);

    // Обновление предыдущего значения фильтра для следующего вызова
    *previousValue = outputValue;

    return outputValue;
}

//! \brief Фильтр высоких частот первого порядка
//! \param[in] inputValue - входное значение фильтра
//! \param[in] alphaCoeff - коэффициент фильтра (0 < alpha < 1)
//! \param[in, out] previousValue - предыдущее входное значение фильтра
//! \return Отфильтрованное значение
static float HighPassFilterFirstOrder(float inputValue, float alphaCoeff, float *previousValue)
{
    // Пояснение. Формула ФВЧ первого порядка:
    // y[n] = alpha * (x[n] - x[n-1] + y[n-1])
    // где y[n] - текущее выходное значение,
    //     y[n-1] - предыдущее выходное значение,
    //     x[n] - текущее входное значение,
    //     x[n-1] - предыдущее входное значение,
    //     alpha - коэффициент фильтра (чем меньше, тем сильнее сглаживание)

    float outputValue = alphaCoeff * (inputValue - (*previousValue) + outputValue);

    // Обновление предыдущего значения фильтра для следующего вызова
    *previousValue = inputValue;

    return outputValue;
}

//! \brief Разделение сигнала на полосы и применение коэффициентов усиления
//! \param[in] sample - входной сэмпл
//! \param[in,out] state - состояние фильтров для канала
//! \return Обработанный сэмпл
static int16_t ProcessChannel(int16_t sample, FilterState *state)
{
    // Текущие значения частот
    float currentEqualizerBands[EQUALIZER_BANDS_QUANTITY] = { 0.0f, 0.0f, 0.0f };

    // Обработанные значения частот
    float processedEqualizerBands[EQUALIZER_BANDS_QUANTITY] = { 0.0f, 0.0f, 0.0f };

    // Обработанный сэмпл
    float outputSample = 0.0f;

    // Преобразование входного сэмпла из целочисленного в вещественный тип
    float inputSample = (float) sample;

    // Выделение низких частот с помощью ФНЧ
    currentEqualizerBands[LOW_FREQUENCIES] = LowPassFilterFirstOrder(inputSample, lowAlphaCoeff, &state->lowFrequencyPrevious);
    
    // Выделение высоких частот с помощью ФВЧ
    currentEqualizerBands[HIGH_FREQUENCIES] = HighPassFilterFirstOrder(inputSample, highAlphaCoeff, &state->highFrequencyPrevious);
    
    // Выделение средних частот (исходный сигнал минус низкие и высокие частоты)
    currentEqualizerBands[MID_FREQUENCIES] = inputSample - currentEqualizerBands[LOW_FREQUENCIES] - currentEqualizerBands[HIGH_FREQUENCIES];
    
    for (uint8_t equalizerBandIndex = 0; equalizerBandIndex < EQUALIZER_BANDS_QUANTITY; equalizerBandIndex++)
    {
        // Применение усиления к каждой полосе
        processedEqualizerBands[equalizerBandIndex] = currentEqualizerBands[equalizerBandIndex] * frequenciesGainCoeff[equalizerBandIndex];

        // Смешивание обработанных полос (суммирование всех частотных компонент)
        outputSample += processedEqualizerBands[equalizerBandIndex];
    }
    
    // Ограничение громкости для 16-ти битного сэмпла
    if (outputSample > MAX_VOLUME_SAMPLE)
    {
        outputSample = MAX_VOLUME_SAMPLE;
    }
    else if (outputSample < MIN_VOLUME_SAMPLE)
    {
        outputSample = MIN_VOLUME_SAMPLE;
    }
    
    // Приведение типа сэмпла обратно к целочисленному
    return (int16_t) outputSample;
}

//! \brief Инициализация модуля звуковых пресетов
void SoundPresets_Init(void)
{
    // Пояснение. Расчет аргумента экспоненты для фильтра низких/высоких частот:
    // -2 * pi * Fc / Fs. Отрицательный знак нужен для получения exp(-x)
    // при вычислении alpha = 1 - exp(-x)

    float omegaLow = -PHASE_CYCLE * pi * LOW_CROSSOVER_FREQUENCY / SAMPLE_RATE;
    float omegaHigh = -PHASE_CYCLE * pi * HIGH_CROSSOVER_FREQUENCY / SAMPLE_RATE;
    
    // Пояснение. Расчет коэффициента альфа для ФНЧ по формуле:
    // alpha = 1 - exp(omegaLow), где 1 - единица, обеспечивающая значение
    // alpha в диапазоне (0, 1), exp(omegaLow) - экспонента отрицательного
    // аргумента (значение от 0 до 1). Для ФВЧ аналогично.
    
    lowAlphaCoeff = ALPHA_COEFF_ONE - expf(omegaLow);
    highAlphaCoeff = ALPHA_COEFF_ONE - expf(omegaHigh);
    
    #ifdef DEBUG_INFO_SOUND_PRESETS

        Serial.printf("Инициализация: коэффициент ФНЧ = %.4f, коэффициент ФВЧ = %.4f\r\n", lowAlphaCoeff, highAlphaCoeff);

    #endif // DEBUG_INFO_SOUND_PRESETS
    
    // Чтение сохраненного пресета из flash-памяти
    currentPreset = ReadSoundPresetFromFlash();
    
    // Обновление коэффициентов усиления в соответствии с загруженным пресетом
    SoundPresets_UpdateGains();
}

//! \brief Применение эквалайзера к аудиосэмплу
//! \param[in] inputSample - входной сэмпл
//! \param[in] channel - номер канала (0 - левый, 1 - правый)
//! \return Обработанный сэмпл
int16_t SoundPresets_ProcessSample(int16_t inputSample, uint8_t channel)
{
    if (0 == channel) // Левый канал
    {
        return ProcessChannel(inputSample, &leftChannel);
    }
    else // Правый канал
    {
        return ProcessChannel(inputSample, &rightChannel);
    }
}

//! \brief Установка пресета эквалайзера
//! \param[in] preset - пресет для установки
void SoundPresets_SetPreset(EqualizerPreset preset)
{
    // Защита от выхода за допустимый диапазон
    if (EQUALIZER_PRESETS_QUANTITY <= preset)
    {
        return;
    }
    
    // Обновление текущего пресета
    currentPreset = preset;

    // Обновление коэффициентов усиления в соответствии с текущим пресетом
    SoundPresets_UpdateGains();
    
    // Запись текущего пресета во flash-память
    WriteSoundPresetToFlash();
}

//! \brief Переключение на следующий пресет
static void SoundPresets_SwitchNextPreset(void)
{
    if (((EqualizerPreset) (EQUALIZER_PRESETS_QUANTITY - 1)) == currentPreset) // Если установлен последний пресет
    {
        // Переключение на первый пресет
        SoundPresets_SetPreset(EQUALIZER_PRESET_NORMAL);
    }
    else // Если установлен не последний пресет
    {
        // Переключение на следующий пресет
        SoundPresets_SetPreset((EqualizerPreset) (currentPreset + 1));
    }
}

//! \brief Переключение на предыдущий пресет
static void SoundPresets_SwitchPreviousPreset(void)
{   
    if (EQUALIZER_PRESET_NORMAL == currentPreset) // Если установлен первый пресет
    {
        // Переключение на последний пресет
        SoundPresets_SetPreset((EqualizerPreset) (EQUALIZER_PRESETS_QUANTITY - 1));
    }
    else // Если установлен не первый пресет
    {
        // Переключение на предыдущий пресет
        SoundPresets_SetPreset((EqualizerPreset) (currentPreset - 1));
    }
}

//! \brief Управление пресетами эквалайзера
void SoundPresets_Control(void)
{
    // Получение адреса массива с количеством устойчивых нажатий на кнопки
    uint8_t *pButtonsPressCount = ButtonsDriver_GetButtonsPressCountPointer();
    
    // Получение адреса массива со статусами завершения серий нажатий 
    ButtonPressSeriesStatus *pButtonSeriesStatus = ButtonsDriver_GetButtonsPressSeriesStatusPointer();
    
    // Если завершена серия нажатий на кнопку управления пресетами
    if (BUTTON_PRESS_SERIES_FINISHED == pButtonSeriesStatus[BUTTON_SOUND_PRESET_CONTROL])
    {
        #ifdef DEBUG_INFO_BUTTON_SOUND_PRESETS

            Serial.printf("Количество нажатий на кнопку управления пресетами: %d\r\n", pButtonsPressCount[BUTTON_SOUND_PRESET_CONTROL]);

        #endif // DEBUG_INFO_BUTTON_SOUND_PRESETS
        
        if (ONE_PRESS == pButtonsPressCount[BUTTON_SOUND_PRESET_CONTROL])           // Зафиксировано одно нажатие на кнопку
        {
            // Переключение на следующий пресет
            SoundPresets_SwitchNextPreset();
        }
        else if (TWO_PRESS == pButtonsPressCount[BUTTON_SOUND_PRESET_CONTROL])      // Зафиксировано два нажатия на кнопку
        {
            // Переключение на предыдущий пресет
            SoundPresets_SwitchPreviousPreset();
        }
        else if (THREE_PRESS == pButtonsPressCount[BUTTON_SOUND_PRESET_CONTROL])    // Зафиксировано три нажатия на кнопку
        {
            // Установка пресета по умолчанию
            SoundPresets_SetPreset(EQUALIZER_PRESET_NORMAL);
        }
        
        // Сброс статуса завершения серии нажатий
        pButtonSeriesStatus[BUTTON_SOUND_PRESET_CONTROL] = BUTTON_PRESS_SERIES_NONE;
        
        // Сброс количества нажатий
        pButtonsPressCount[BUTTON_SOUND_PRESET_CONTROL] = 0;
    }
}

//! \brief Получение текущего пресета эквалайзера
//! \return Текущий пресет
EqualizerPreset SoundPresets_GetCurrentPreset(void)
{
    return currentPreset;
}