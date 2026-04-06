#ifndef INC_WATCH_DOG_H_
#define INC_WATCH_DOG_H_

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Инициализация сторожевого таймера
void WatchDog_Init(void);

//! \brief Сброс сторожевого таймера
void WatchDog_Reset(void);

#ifdef __cplusplus
}
#endif

#endif // INC_WATCH_DOG_H_