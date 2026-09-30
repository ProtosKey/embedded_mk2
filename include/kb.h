#ifndef INC_KEYBOARD_H_
#define INC_KEYBOARD_H_

#include "main.h"

// Маски строк для регистра CONFIG PCA9538: 0 - строка выход (выдаёт 0)
#define ROW1 0xFE // верхний ряд
#define ROW2 0xFD
#define ROW3 0xFB
#define ROW4 0xF7 // нижний ряд

#define KB_KEYS 12

HAL_StatusTypeDef KB_Init(void);
// Неблокирующий опрос, вызывать из главного цикла как можно чаще.
// Возвращает код нажатой кнопки 1..12 (слева направо, сверху вниз) или 0
uint8_t KB_Poll(void);

#endif /* INC_KEYBOARD_H_ */
