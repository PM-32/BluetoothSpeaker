#include <Arduino.h>

#include "AdcMeasurements.h"
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

    // Инициализация I2S1 и A2DP
    SoundControl_Init();

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

    // Вывод информации о положении ручки потенциометра
    // управления яркостью светодиодной матрицы в процентах
    #ifdef DEBUG_INFO_POTENTIOMETER_BRIGHT_CONTROL_PERCENTS

        // Получение адреса массива с отсчетами АЦП в процентах
        uint8_t *pAdcCountsInPercents = AdcMeasurements_GetAdcCountsInPercentsPointer();

        Serial.printf("Яркость: %u%%\r\n", pAdcCountsInPercents[POTENTIOMETER_BRIGHT_CONTROL]);

    #endif // DEBUG_INFO_POTENTIOMETER_BRIGHT_CONTROL_PERCENTS

    // Вывод информации о состоянии кнопки
    // инициализации Bluetooth на терминал при необходимости
    #ifdef DEBUG_INFO_BUTTON_INIT_BLUETOOTH_STATE
    
        // Получение адреса массива с количеством устойчивых нажатий на кнопки
        uint8_t *pButtonsPressCount = ButtonsDriver_GetButtonsPressCountPointer();

        // Получение адреса массива со статусами завершения серий нажатий 
        ButtonPressSeriesStatus *pButtonSeriesStatus = ButtonsDriver_GetButtonsPressSeriesStatusPointer();

        // Если завершена серия нажатий на кнопку управления звука
        if (BUTTON_PRESS_SERIES_FINISHED == pButtonSeriesStatus[BUTTON_INIT_BLUETOOTH])
        {
            if (ONE_PRESS == pButtonsPressCount[BUTTON_INIT_BLUETOOTH])      // Зафиксировано одно нажатие на кнопку
            {
                Serial.println("Кнопка инициализации Bluetooth - действие на первое нажатие");
            }
            else if (TWO_PRESS == pButtonsPressCount[BUTTON_INIT_BLUETOOTH]) // Зафиксировано два нажатия на кнопку
            {
                Serial.println("Кнопка инициализации Bluetooth - действие на второе нажатие");
            }

            // Сброс статуса завершения серии нажатий на кнопку управления звука
            pButtonSeriesStatus[BUTTON_INIT_BLUETOOTH] = BUTTON_PRESS_SERIES_NONE;

            // Сброс количества нажатий на кнопку управления звука
            pButtonsPressCount[BUTTON_INIT_BLUETOOTH] = 0;
        }

    #endif // DEBUG_INFO_BUTTON_INIT_BLUETOOTH_STATE

    // Задержка для вывода отладочной информации в терминал
    UserTimer_Delay(100);
}