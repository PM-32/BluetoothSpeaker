#include <BluetoothA2DPSink.h>
#include "esp_bt.h"
#include "esp_bt_main.h"

#include "BluetoothConnectionControl.h"
#include "ButtonsDriver.h"
#include "LedsDriver.h"
#include "SoundControl.h"
#include "UserTimer.h"

#define DEBUG_INFO_BLUETOOTH_CONNECTION                // Вывод информации о состоянии подключения

#define LED_BLINK_PERIOD_CONNECTED      10000           //!< Период мигания светодиода при подключении (1 секунда)
#define LED_BLINK_PERIOD_DISCONNECTED   5000            //!< Период мигания светодиода при отключении (0.5 секунды)

static BluetoothConnectionState connectionState = BLUETOOTH_DISCONNECTED;   //!< Текущее состояние подключения
static uint32_t lastLedUpdateTime = 0;                                      //!< Время последнего обновления светодиода (в периодах таймера)
static LedState currentLedState = LED_OFF;                                  //!< Текущее состояние светодиода (из LedsDriver.h)

// // Указатель на объект A2DP (объявлен в SoundControl.cpp)
// extern BluetoothA2DPSink a2dp_sink;

//! \brief Обновление состояния светодиода в зависимости от подключения
static void BluetoothConnectionControl_UpdateLedState(void)
{
    // Если подключение установлено
    if (BLUETOOTH_CONNECTED == connectionState)
    {
        // Светодиод должен гореть постоянно
        if (LED_ON != currentLedState)
        {
            LedsDriver_SetLedState(BLUETOOTH_STATUS_LED_PIN, LED_ON);
            currentLedState = LED_ON;
        }
        return;
    }
    
    // Если подключение отсутствует - светодиод мигает
    uint32_t blinkPeriod = LED_BLINK_PERIOD_DISCONNECTED;
    
    // Проверяем, прошло ли время для смены состояния светодиода
    if ((UserTimer_GetCounterTime() - lastLedUpdateTime) > blinkPeriod)
    {
        // Меняем состояние светодиода
        if (LED_OFF == currentLedState)
        {
            LedsDriver_SetLedState(BLUETOOTH_STATUS_LED_PIN, LED_ON);
            currentLedState = LED_ON;
        }
        else
        {
            LedsDriver_SetLedState(BLUETOOTH_STATUS_LED_PIN, LED_OFF);
            currentLedState = LED_OFF;
        }
        
        // Обновляем время последнего изменения
        lastLedUpdateTime = UserTimer_GetCounterTime();
    }
}

//! \brief Инициализация модуля управления Bluetooth-подключением
void BluetoothConnectionControl_Init(void)
{
    connectionState = BLUETOOTH_DISCONNECTED;
    lastLedUpdateTime = UserTimer_GetCounterTime();
    
    // Инициализация состояния светодиода (выключен)
    LedsDriver_SetLedState(BLUETOOTH_STATUS_LED_PIN, LED_OFF);
    currentLedState = LED_OFF;
}

//! \brief Обработка кнопки инициализации Bluetooth
void BluetoothConnectionControl_HandleButton(void)
{
    // Получение адреса массива с количеством устойчивых нажатий на кнопки
    uint8_t *pButtonsPressCount = ButtonsDriver_GetButtonsPressCountPointer();

    // Получение адреса массива со статусами завершения серий нажатий 
    ButtonPressSeriesStatus *pButtonSeriesStatus = ButtonsDriver_GetButtonsPressSeriesStatusPointer();

    // Если завершена серия нажатий на кнопку инициализации Bluetooth
    if (BUTTON_PRESS_SERIES_FINISHED == pButtonSeriesStatus[BUTTON_INIT_BLUETOOTH])
    {
        // Одно нажатие: включение режима сопряжения (устройство становится видимым)
        if (ONE_PRESS == pButtonsPressCount[BUTTON_INIT_BLUETOOTH])
        {
            #ifdef DEBUG_INFO_BLUETOOTH_CONNECTION

                Serial.println("»> ОДНО НАЖАТИЕ: Включение режима сопряжения");

            #endif // DEBUG_INFO_BLUETOOTH_CONNECTION

            // Делаем устройство видимым и доступным для подключения
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        }
        // Два нажатия: сброс текущего подключения и перезапуск
        else if (TWO_PRESS == pButtonsPressCount[BUTTON_INIT_BLUETOOTH])
        {
            #ifdef DEBUG_INFO_BLUETOOTH_CONNECTION

                Serial.println("»> ДВА НАЖАТИЯ: Сброс подключения и перезапуск");

            #endif // DEBUG_INFO_BLUETOOTH_CONNECTION

            BluetoothA2DPSink *pBluetoothA2DPSink = SoundControl_GetA2DPSinkPointer();

            // // Принудительно отключаем текущее устройство
            // a2dp_sink.end();

            pBluetoothA2DPSink->end();
            
            // Небольшая задержка для завершения процессов
            UserTimer_Delay(1000);
            
            // // Перезапускаем A2DP
            // a2dp_sink.start("MyMusic");

            pBluetoothA2DPSink->start("MyMusic");
        }

        // Сброс статуса завершения серии нажатий
        pButtonSeriesStatus[BUTTON_INIT_BLUETOOTH] = BUTTON_PRESS_SERIES_NONE;
        pButtonsPressCount[BUTTON_INIT_BLUETOOTH] = 0;
    }
}

//! \brief Обновление индикации состояния подключения
void BluetoothConnectionControl_UpdateLed(void)
{
    BluetoothA2DPSink *pBluetoothA2DPSink = SoundControl_GetA2DPSinkPointer();

    // // Проверяем текущее состояние подключения и преобразуем в enum
    // BluetoothConnectionState newState = (BluetoothConnectionState) a2dp_sink.is_connected();

    // Проверяем текущее состояние подключения и преобразуем в enum
    BluetoothConnectionState newState = (BluetoothConnectionState) pBluetoothA2DPSink->is_connected();
    
    // Если состояние изменилось
    if (newState != connectionState)
    {
        connectionState = newState;
        
        // При подключении сразу включаем светодиод, при отключении - выключаем
        if (BLUETOOTH_CONNECTED == connectionState)
        {
            LedsDriver_SetLedState(BLUETOOTH_STATUS_LED_PIN, LED_ON);
            currentLedState = LED_ON;
        }
        else
        {
            LedsDriver_SetLedState(BLUETOOTH_STATUS_LED_PIN, LED_OFF);
            currentLedState = LED_OFF;
        }
        
        // Сбрасываем таймер мигания
        lastLedUpdateTime = UserTimer_GetCounterTime();
        
        #ifdef DEBUG_INFO_BLUETOOTH_CONNECTION
        
            Serial.printf("Состояние подключения изменилось: %s\r\n", 
                         (BLUETOOTH_CONNECTED == connectionState) ? "ПОДКЛЮЧЕНО" : "ОТКЛЮЧЕНО");
        
        #endif // DEBUG_INFO_BLUETOOTH_CONNECTION
    }
    
    // Если подключение отсутствует, обновляем состояние мигающего светодиода
    if (BLUETOOTH_DISCONNECTED == connectionState)
    {
        BluetoothConnectionControl_UpdateLedState();
    }
}