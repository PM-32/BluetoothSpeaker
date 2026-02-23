#ifndef INC_CALLBACKS_H_
#define INC_CALLBACKS_H_

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Получение адреса коллбэк-функции прерывания по переполнению таймера
//! \return Адрес коллбэк-функции прерывания по переполнению таймера
void * Callbacks_GetTim0PeriodElapsedCallbackFunctionPointer(void);

#ifdef __cplusplus
}
#endif

#endif // INC_CALLBACKS_H_