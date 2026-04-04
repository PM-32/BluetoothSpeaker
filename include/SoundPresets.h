#ifndef INC_SOUND_PRESETS_H_
#define INC_SOUND_PRESETS_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// //! \brief Пресеты эквалайзера
// typedef enum
// {
//     EQ_PRESET_NORMAL = 0,       //!< Плоская характеристика (0, 0, 0) dB
//     EQ_PRESET_BASS,             //!< Усиление низких частот (+6, 0, -2) dB
//     EQ_PRESET_TREBLE,           //!< Усиление высоких частот (-2, 0, +6) dB
//     EQ_PRESET_VOCAL,            //!< Выделение голоса (-2, +4, -1) dB
//     EQ_PRESET_ROCK,             //!< Агрессивный звук (+3, +2, +3) dB
//     EQ_PRESETS_QUANTITY         //!< Общее количество пресетов
// } EqPreset;

//! \brief Пресеты эквалайзера
typedef enum
{
    EQ_PRESET_NORMAL = 0,       //!< Плоская характеристика
    EQ_PRESET_BASS,             //!< Усиление низких частот
    EQ_PRESET_TREBLE,           //!< Усиление высоких частот
    EQ_PRESET_VOCAL,            //!< Выделение голоса
    EQ_PRESET_ROCK,             //!< Рок-музыка
    EQ_PRESET_ELECTRO,          //!< Электронная музыка
    EQ_PRESETS_QUANTITY         //!< Общее количество пресетов (6)
} EqPreset;

//! \brief Инициализация модуля звуковых пресетов
void SoundPresets_Init(void);

//! \brief Применение эквалайзера к аудиосэмплу
//! \param[in] sample - входной сэмпл
//! \param[in] channel - номер канала (0=левый, 1=правый)
//! \return Обработанный сэмпл
int16_t SoundPresets_ProcessSample(int16_t sample, uint8_t channel);

//! \brief Установка пресета эквалайзера
//! \param[in] preset - пресет из перечисления EqPreset
void SoundPresets_SetPreset(EqPreset preset);

//! \brief Получение текущего пресета эквалайзера
//! \return Текущий пресет
EqPreset SoundPresets_GetCurrentPreset(void);

//! \brief Переключение на следующий пресет
void SoundPresets_NextPreset(void);

//! \brief Переключение на предыдущий пресет
void SoundPresets_PreviousPreset(void);

//! \brief Сброс к пресету Normal
void SoundPresets_ResetToNormal(void);

//! \brief Управление пресетами эквалайзера (обработка нажатий кнопки)
void SoundPresets_Control(void);

#ifdef __cplusplus
}
#endif

#endif // INC_SOUND_PRESETS_H_