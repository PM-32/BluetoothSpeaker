#ifndef INC_AMPLIFIER_AND_DAC_CONTROL_H_
#define INC_AMPLIFIER_AND_DAC_CONTROL_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

//! \brief Инициализация модуля для управления включением усилителя и ЦАП
void AmplifierAndDacControl_Init(void);

//! \brief Включение усилителя и ЦАП с задержкой
void AmplifierAndDacControl_TurnOnChipsAfterDelay(void);

#ifdef __cplusplus
}
#endif

#endif // INC_AMPLIFIER_AND_DAC_CONTROL_H_