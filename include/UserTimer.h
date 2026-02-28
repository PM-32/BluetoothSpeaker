#ifndef INC_USER_TIMER_H_
#define INC_USER_TIMER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Инициализация таймера для отсчета интервалов
void UserTimer_InitTimer(void);

//! \brief Инкремент счетчика работы программы
void UserTimer_IncrementCounter(void);

//! \brief Запуск таймера 0
void UserTimer_StartTim0(void);

//! \brief Получение текущего значения
//!        счетчика времени работы программы
//! \return Значение счетчика времени работы программы
uint32_t UserTimer_GetCounterTime(void);

#ifdef __cplusplus
}
#endif

#endif // INC_USER_TIMER_H_