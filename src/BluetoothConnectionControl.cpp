#include <BluetoothA2DPSink.h>

#include "esp_bt.h"
#include "esp_bt_main.h"

#include "AudioNotifications.h"
#include "BluetoothConnectionControl.h"
#include "ButtonsDriver.h"
#include "LedsDriver.h"
#include "SoundControl.h"
#include "UserTimer.h"

// #define DEBUG_INFO_BLUETOOTH_CONNECTION             // Вывод информации о состоянии Bluetooth-подключения

#define LED_BLINK_PERIOD_DISCONNECTED   8000        //!< Период мигания светодиода при отключении
                                                    //!< Bluetooth в количестве периодов таймера 0 (800 мс)

//! \brief Состояние подключения Bluetooth
typedef enum
{
    BLUETOOTH_DISCONNECTED = 0,     //!< Подключение отсутствует
    BLUETOOTH_CONNECTED             //!< Подключение установлено
} BluetoothConnectionState;

//! \brief Инициализация модуля управления Bluetooth-подключением
void BluetoothConnectionControl_Init(void)
{
    // Выключение светодиода
    LedsDriver_SetLedState(BLUETOOTH_STATUS_LED_PIN, LED_OFF);
}

//! \brief Управление Bluetooth-подключением
void BluetoothConnectionControl_Execution(void)
{
    // Получение адреса массива с количеством устойчивых нажатий на кнопки
    uint8_t *pButtonsPressCount = ButtonsDriver_GetButtonsPressCountPointer();

    // Получение адреса массива со статусами завершения серий нажатий 
    ButtonPressSeriesStatus *pButtonSeriesStatus = ButtonsDriver_GetButtonsPressSeriesStatusPointer();

    // Если завершена серия нажатий на кнопку инициализации Bluetooth
    if (BUTTON_PRESS_SERIES_FINISHED == pButtonSeriesStatus[BUTTON_BLUETOOTH_CONNECTION_CONTROL])
    {
        if (ONE_PRESS == pButtonsPressCount[BUTTON_BLUETOOTH_CONNECTION_CONTROL])             // Зафиксировано одно нажатие на кнопку
        {
            // Устройство становится видимым и доступным для подключения
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

            #ifdef DEBUG_INFO_BLUETOOTH_CONNECTION

                Serial.println("Включение режима сопряжения Bluetooth");

            #endif // DEBUG_INFO_BLUETOOTH_CONNECTION
        }
        else if (TWO_PRESS == pButtonsPressCount[BUTTON_BLUETOOTH_CONNECTION_CONTROL])        // Зафиксировано два нажатия на кнопку
        {
            // Получение адреса объекта BluetoothA2DPSink
            BluetoothA2DPSink *pBluetoothA2DPSink = SoundControl_GetA2DPSinkPointer();

            // Отключение текущего подключенного устройства
            pBluetoothA2DPSink->end();
            
            // Задержка для завершения процесса отключения
            UserTimer_Delay(1000);

            // Перезапуск протокола A2DP
            pBluetoothA2DPSink->start(SPEAKER_NAME);

            #ifdef DEBUG_INFO_BLUETOOTH_CONNECTION

                Serial.println("Сброс Bluetooth-подключения и перезапуск");

            #endif // DEBUG_INFO_BLUETOOTH_CONNECTION
        }

        // Сброс статуса завершения серии нажатий
        pButtonSeriesStatus[BUTTON_BLUETOOTH_CONNECTION_CONTROL] = BUTTON_PRESS_SERIES_NONE;
        pButtonsPressCount[BUTTON_BLUETOOTH_CONNECTION_CONTROL] = 0;
    }
}

//! \brief Индикация состояния Bluetooth-подключения
void BluetoothConnectionControl_IndicateConnectionStatus(void)
{
    // Время последней смены состояния светодиода
    static uint32_t lastLedChangeStateTime = 0;

    // Текущее состояние Bluetooth-подключения
    static BluetoothConnectionState connectionState = BLUETOOTH_DISCONNECTED;

    // Получение адреса объекта BluetoothA2DPSink
    BluetoothA2DPSink *pBluetoothA2DPSink = SoundControl_GetA2DPSinkPointer();

    // Получение нового состояния подключения
    BluetoothConnectionState newConnectionState = (BluetoothConnectionState) pBluetoothA2DPSink->is_connected();

    // Если состояние подключения изменилось
    if (newConnectionState != connectionState)
    {
        // Обновление текущего состояния подключения
        connectionState = newConnectionState;

        // Если подключение установлено
        if (BLUETOOTH_CONNECTED == connectionState)
        {
            // Включение светодиода
            LedsDriver_SetLedState(BLUETOOTH_STATUS_LED_PIN, LED_ON);
            
            AudioNotifications_Play(NOTIFICATION_CONNECT);
        }
        else
        {
            AudioNotifications_Play(NOTIFICATION_DISCONNECT);
        }

        // Обновление времени последней смены состояния светодиода
        lastLedChangeStateTime = UserTimer_GetCounterTime();

        #ifdef DEBUG_INFO_BLUETOOTH_CONNECTION
        
            Serial.printf("Состояние соединения изменилось: %s\r\n",
                         (BLUETOOTH_CONNECTED == connectionState) ? "подключено" : "отключено");
        
        #endif // DEBUG_INFO_BLUETOOTH_CONNECTION
    }

    // Если подключение отсутствует
    if (BLUETOOTH_DISCONNECTED == connectionState)
    {
        // Переключение состояния светодиода при
        // достижении периода LED_BLINK_PERIOD_DISCONNECTED
        if ((UserTimer_GetCounterTime() - lastLedChangeStateTime) > LED_BLINK_PERIOD_DISCONNECTED)
        {
            // Смена состояния светодиода
            LedsDriver_ToggleLed(BLUETOOTH_STATUS_LED_PIN);

            // Обновление времени последней смены состояния светодиода
            lastLedChangeStateTime = UserTimer_GetCounterTime();
        }
    }
}