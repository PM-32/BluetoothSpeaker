#ifndef COMMON_FUNCTIONS_H_
#define COMMON_FUNCTIONS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Состояние пина
typedef enum
{
    PIN_RESET = 0,  //!< Пин в состоянии лог. "0"
    PIN_SET         //!< Пин в состоянии лог. "1"
} PinState;

//! \brief Чтение состояния пина
//! \param[in] gpioPin - номер пина
PinState CommonFunctions_GpioGetState(uint8_t gpioPin);

//! \brief Установка состояния пина
//! \param[in] gpioPin - номер пина
//! \param[in] bitState - логический уровень
void CommonFunctions_GpioSetState(uint8_t gpioPin, PinState bitState);

//! \brief Смена состояния пина
//! \param[in] gpioPin - номер пина
void CommonFunctions_GpioToggleState(uint8_t gpioPin);

#ifdef __cplusplus
}
#endif

#endif // COMMON_FUNCTIONS_H_