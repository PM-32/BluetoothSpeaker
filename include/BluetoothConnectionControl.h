#ifndef INC_BLUETOOTH_CONNECTION_CONTROL_H_
#define INC_BLUETOOTH_CONNECTION_CONTROL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Инициализация модуля управления Bluetooth-подключением
void BluetoothConnectionControl_Init(void);

//! \brief Управление Bluetooth-подключением
void BluetoothConnectionControl_Execution(void);

//! \brief Индикация состояния Bluetooth-подключения
void BluetoothConnectionControl_IndicateConnectionStatus(void);

#ifdef __cplusplus
}
#endif

#endif // INC_BLUETOOTH_CONNECTION_CONTROL_H_