#include <Arduino.h>

#include "AdcMeasurements.h"
#include "BluetoothConnectionControl.h"
#include "ButtonsDriver.h"
#include "LedsDriver.h"
#include "SoundControl.h"
#include "UserTimer.h"

// #define DEBUG_INFO_BUTTON_INIT_BLUETOOTH_STATE               // Вывод информации о состоянии кнопки
//                                                              // инициализации Bluetooth на терминал
// #define DEBUG_INFO_POTENTIOMETER_BRIGHT_CONTROL_PERCENTS     // Вывод информации о положении ручки потенциометра
//                                                              // управления яркостью светодиодной матрицы в процентах

//! \brief Инициализация
void setup()
{
    // Запуск UART для вывода отладочной информации
    Serial.begin(9600);
    
    // Инициализация кнопок
    ButtonsDriver_Init();

    // Инициализация светодиода
    LedsDriver_Init();

    // Инициализация АЦП
    AdcMeasurements_Init();

    // Инициализация I2S1 и протокола A2DP
    SoundControl_Init();

    // Инициализация модуля управления Bluetooth-подключением
    BluetoothConnectionControl_Init();

    // Инициализация таймера для отсчета интервалов
    UserTimer_InitTimer();

    // Запуск таймера 0
    UserTimer_StartTim0();
}

//! \brief Основной цикл программы
void loop()
{
    // Периодический опрос каналов АЦП
    AdcMeasurements_Pollings();

    // Управление воспроизведением звука
    SoundControl_Playback();

    // Управление громкостью звука
    SoundControl_Volume();

    // Управление Bluetooth-подключением
    BluetoothConnectionControl_Execution();

    // Индикация состояния Bluetooth-подключения
    BluetoothConnectionControl_IndicateConnectionStatus();

    // Вывод информации о положении ручки потенциометра
    // управления яркостью светодиодной матрицы в процентах
    #ifdef DEBUG_INFO_POTENTIOMETER_BRIGHT_CONTROL_PERCENTS

        // Получение адреса массива с отсчетами АЦП в процентах
        uint8_t *pAdcCountsInPercents = AdcMeasurements_GetAdcCountsInPercentsPointer();

        Serial.printf("Яркость: %u%%\r\n", pAdcCountsInPercents[POTENTIOMETER_BRIGHT_CONTROL]);

    #endif // DEBUG_INFO_POTENTIOMETER_BRIGHT_CONTROL_PERCENTS

    // Задержка для вывода отладочной информации в терминал
    UserTimer_Delay(100);
}