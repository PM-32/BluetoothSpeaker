#ifndef SOUND_CONTROL_H_
#define SOUND_CONTROL_H_

//! \brief Инициализация I2S1
void SoundControl_Init(void);

//! \brief Управление воспроизведением звука
//! \note Управление паузой/воспроизведением звука,
//!       переключением треков с помощью кнопки
void SoundControl_Playback(void);

#endif // SOUND_CONTROL_H_