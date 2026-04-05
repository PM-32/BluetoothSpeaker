#ifndef INC_SOUND_PRESETS_H_
#define INC_SOUND_PRESETS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Пресеты эквалайзера
typedef enum
{
    EQUALIZER_PRESET_NORMAL = 0,    //!< Плоская характеристика
    EQUALIZER_PRESET_BASS,          //!< Усиление низких частот
    EQUALIZER_PRESET_TREBLE,        //!< Усиление высоких частот
    EQUALIZER_PRESET_VOCAL,         //!< Усиление средних частот (вокал)
    EQUALIZER_PRESET_ROCK,          //!< Рок-музыка
    EQUALIZER_PRESET_ELECTRO,       //!< Электронная музыка
    EQUALIZER_PRESETS_QUANTITY      //!< Количество пресетов
} EqualizerPreset;

//! \brief Инициализация модуля звуковых пресетов
void SoundPresets_Init(void);

//! \brief Применение эквалайзера к аудиосэмплу
//! \param[in] sample - входной сэмпл
//! \param[in] channel - номер канала (0 - левый, 1 - правый)
//! \return Обработанный сэмпл
int16_t SoundPresets_ProcessSample(int16_t sample, uint8_t channel);

//! \brief Управление пресетами эквалайзера
void SoundPresets_Control(void);

#ifdef __cplusplus
}
#endif

#endif // INC_SOUND_PRESETS_H_