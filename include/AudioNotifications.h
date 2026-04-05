#ifndef INC_AUDIO_NOTIFICATIONS_H_
#define INC_AUDIO_NOTIFICATIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Типы звуковых уведомлений
typedef enum
{
    NOTIFICATION_CONNECT = 0,       //!< Подключение Bluetooth
    NOTIFICATION_DISCONNECT         //!< Отключение Bluetooth
} NotificationType;

//! \brief Инициализация звуковых уведомлений
void AudioNotifications_Init(void);

//! \brief Воспроизведение звукового уведомления
//! \param[in] notificationType - тип уведомления
void AudioNotifications_Play(NotificationType notificationType);

//! \brief Обновление состояния воспроизведения
void AudioNotifications_Update(void);

#ifdef __cplusplus
}
#endif

#endif // INC_AUDIO_NOTIFICATIONS_H_