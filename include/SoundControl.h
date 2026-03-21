#ifndef SOUND_CONTROL_H_
#define SOUND_CONTROL_H_

#include <AudioTools.h>
#include <BluetoothA2DPSink.h>

#define SPEAKER_NAME    "MyMusic"       //!< Название колонки при подключении Bluetooth

//! \brief Инициализация I2S1 и протокола A2DP
void SoundControl_Init(void);

//! \brief Управление воспроизведением звука (кнопки)
void SoundControl_Playback(void);

//! \brief Регулировка громкости (потенциометр + синхронизация со смартфоном)
void SoundControl_Volume(void);

//! \brief Получение указателя на объект BluetoothA2DPSink
//! \return Указатель на объект BluetoothA2DPSink
BluetoothA2DPSink * SoundControl_GetA2DPSinkPointer(void);

#endif // SOUND_CONTROL_H_