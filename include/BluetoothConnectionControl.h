#ifndef INC_BLUETOOTH_CONNECTION_CONTROL_H_
#define INC_BLUETOOTH_CONNECTION_CONTROL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Состояние подключения Bluetooth
typedef enum
{
    BLUETOOTH_DISCONNECTED = 0,     //!< Подключение отсутствует
    BLUETOOTH_CONNECTED             //!< Подключение установлено
} BluetoothConnectionState;

//! \brief Инициализация модуля управления Bluetooth-подключением
void BluetoothConnectionControl_Init(void);

//! \brief Обработка кнопки инициализации Bluetooth
void BluetoothConnectionControl_HandleButton(void);

//! \brief Обновление индикации состояния подключения
void BluetoothConnectionControl_UpdateLed(void);

#ifdef __cplusplus
}
#endif

#endif // INC_BLUETOOTH_CONNECTION_CONTROL_H_