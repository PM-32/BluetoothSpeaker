#include "esp32-hal-gpio.h"
#include "soc/gpio_reg.h"

#include "AmplifierAndDacControl.h"
#include "CommonFunctions.h"
#include "UserTimer.h"

// В схеме используется усилитель MAX98306 и ЦАП PCM5102A.
// Для уменьшения щелчков при подаче питания на схему
// необходимо выполнить задержку перед включением усилителя
// и задержку перед включением звука на ЦАП.
// Пин XSMT (PCM5102A) подтянут через резистор 10 кОм к шине +3.3 В.
// Пин SHDN (MAX98306) подтянут через резистор 1 кОм к шине GNDPWR.

#define DAC_SOFTMUTE_CONTROL_PIN            22      //!< Пин для управления включением звука на ЦАП (пин XSMT PCM5102A: 0 - звук выключен, 1 - звук включен)
#define AMPLIFIER_SHUTDOWN_CONTROL_PIN      23      //!< Пин для управления включением усилителя (пин SHDN MAX98306: 0 - усилитель выключен, 1 - усилитель включен)

#define AMPLIFIER_TURN_ON()                 (CommonFunctions_GpioSetState(AMPLIFIER_SHUTDOWN_CONTROL_PIN, PIN_SET))    //!< Включение усилителя
#define AMPLIFIER_TURN_OFF()                (CommonFunctions_GpioSetState(AMPLIFIER_SHUTDOWN_CONTROL_PIN, PIN_RESET))  //!< Отключение усилителя

#define SOUND_UNMUTE()                      (CommonFunctions_GpioSetState(DAC_SOFTMUTE_CONTROL_PIN, PIN_SET))    //!< Включение звука через ЦАП
#define SOUND_MUTE()                        (CommonFunctions_GpioSetState(DAC_SOFTMUTE_CONTROL_PIN, PIN_RESET))  //!< Отключение звука через ЦАП

#define DELAY_BEFORE_SET_SOUND_ON           5000    //!< Задержка перед включением звука в количестве периодов таймера 0 (100 мкс * 5000 = 500 мс)
#define DELAY_BEFORE_AMPLIFIER_POWER_ON     5000    //!< Задержка перед подачей питания на усилитель в количестве периодов таймера 0 (100 мкс * 500 = 500 мс)

//! \brief Статус включения усилителя
typedef enum
{
    AMPLIFIER_OFF = 0,      //!< Усилиетль выключен
    AMPLIFIER_ALREADY_ON    //!< Усилитель уже включен
} AmplifierStatus;

//! \brief Статус включения звука через ЦАП
typedef enum
{
    SOUND_MUTE = 0,         //!< Звук выключен
    SOUND_ALREADY_UNMUTE    //!< Звук уже включен
} SoundStatus;

//! \brief Инициализация модуля для управления включением усилителя и ЦАП
void AmplifierAndDacControl_Init(void)
{
    // Настройка пина управления включением звука ЦАП на выход
    pinMode(DAC_SOFTMUTE_CONTROL_PIN, OUTPUT);

    // Настройка пина управления включением усилителя на выход
    pinMode(AMPLIFIER_SHUTDOWN_CONTROL_PIN, OUTPUT);

    // Отключение усилителя
    AMPLIFIER_TURN_OFF();

    // Отключение звука через ЦАП
    SOUND_MUTE();
}

//! \brief Включение усилителя и ЦАП с задержкой
void AmplifierAndDacControl_TurnOnChipsAfterDelay(void)
{
    // Статус включения усилителя
    static AmplifierStatus amplifierStatus = AMPLIFIER_OFF;
    
    // Статус включения звука через ЦАП
    static SoundStatus soundStatus = SOUND_MUTE;

    // Если звук на ЦАП ещё не включен
    if (SOUND_MUTE == soundStatus)
    {
        // Выжидание задержки
        if (UserTimer_GetCounterTime() > DELAY_BEFORE_SET_SOUND_ON)
        {
            // Включение звука через ЦАП
            SOUND_UNMUTE();

            // Установка статуса - звук уже включен
            soundStatus = SOUND_ALREADY_UNMUTE;
        }
    }

    // Если усилитель ещё не включен
    // и звук включен через ЦАП включен
    if ((AMPLIFIER_OFF == amplifierStatus) &&
        (SOUND_ALREADY_UNMUTE == soundStatus))
    {
        // Выжидание задержки
        if (UserTimer_GetCounterTime() > (DELAY_BEFORE_SET_SOUND_ON + DELAY_BEFORE_AMPLIFIER_POWER_ON))
        {
            // Включение усилителя
            AMPLIFIER_TURN_ON();

            // Установка статуса - усилитель уже включен
            amplifierStatus = AMPLIFIER_ALREADY_ON;
        }
    }
}