#ifndef SOUND_CONTROL_H_
#define SOUND_CONTROL_H_

//! \brief Инициализация I2S1 и A2DP
void SoundControl_Init(void);

//! \brief Управление воспроизведением звука (кнопки)
void SoundControl_Playback(void);

//! \brief Регулировка громкости (потенциометр + синхронизация со смартфоном)
void SoundControl_Volume(void);

#endif // SOUND_CONTROL_H_