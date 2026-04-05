#ifndef SOUND_CONTROL_H_
#define SOUND_CONTROL_H_

#include <AudioTools.h>
#include <BluetoothA2DPSink.h>

#define SPEAKER_NAME                "MyMusic"       //!< Название колонки при подключении Bluetooth

#define MAX_VOLUME_SAMPLE           32767.0f        //!< Абсолютный максимум для 16-ти битного сэмпла (положительная полуволна)
#define MIN_VOLUME_SAMPLE           -32768.0f       //!< Абсолютный минимум для 16-ти битного сэмпла (отрицательная полуволна)
#define AUDIO_CHANNELS_QUANTITY     2               //!< Количество каналов (2 канала: правый и левый)
#define SAMPLE_RATE                 44100           //!< Частота дискретизации (Гц)

#define pi                          3.14159265f     //!< Число Пи
#define PHASE_CYCLE                 2.0f            //!< Полный цикл фазы (2 * pi радиан)

//! \brief Инициализация I2S1 и протокола A2DP
void SoundControl_Init(void);

//! \brief Управление воспроизведением звука (кнопки)
void SoundControl_Playback(void);

//! \brief Регулировка громкости (потенциометр + синхронизация со смартфоном)
void SoundControl_Volume(void);

//! \brief Получение указателя на объект BluetoothA2DPSink
//! \return Указатель на объект BluetoothA2DPSink
BluetoothA2DPSink * SoundControl_GetA2DPSinkPointer(void);

//! \brief Получение указателя на объект I2SStream
//! \return Указатель на объект I2SStream
I2SStream * SoundControl_GetI2SStreamPointer(void);

#endif // SOUND_CONTROL_H_