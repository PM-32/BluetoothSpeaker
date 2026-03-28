#ifndef INC_AUDIO_NOTIFICATIONS_H_
#define INC_AUDIO_NOTIFICATIONS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Типы звуковых уведомлений
typedef enum
{
    NOTIFICATION_CONNECT = 0,       //!< Подключение Bluetooth
    NOTIFICATION_DISCONNECT         //!< Отключение Bluetooth
} NotificationType;

//! \brief Инициализация модуля звуковых уведомлений
void AudioNotifications_Init(void);

//! \brief Воспроизведение звукового уведомления
//! \param[in] type - тип уведомления (подключение/отключение)
void AudioNotifications_Play(NotificationType type);

//! \brief Обновление состояния воспроизведения (вызывать в loop)
void AudioNotifications_Update(void);

#ifdef __cplusplus
}
#endif

#endif // INC_AUDIO_NOTIFICATIONS_H_