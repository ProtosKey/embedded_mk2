#include "kb.h"
#include "pca9538.h"
#include <stdbool.h>

#define KBRD_ADDR 0xE2

// Строку выбираем в одном вызове, а читаем в следующем: у строк нет подтяжки,
// и после переключения линиям нужно время, чтобы установиться. Иначе нажатая
// кнопка "видна" и в соседней строке, и одно нажатие читается как несколько
#define KB_ROW_PERIOD_MS 3   // пауза между выбором строки и чтением
#define KB_DEBOUNCE_SCANS 3  // столько полных опросов подряд состояние должно совпасть
#define KB_MULTI 0xFF        // нажато больше одной кнопки

static const uint8_t rows[] = {ROW1, ROW2, ROW3, ROW4};
static const uint8_t cols[] = {0x10, 0x20, 0x40}; // P4..P6: левый, средний, правый

static uint8_t row;       // строка, выбранная сейчас в CONFIG
static uint8_t scan_code; // результат текущего прохода по строкам

static HAL_StatusTypeDef KB_SelectRow(uint8_t r) {
	uint8_t buf = rows[r];
	row = r;
	return PCA9538_Write_Register(KBRD_ADDR, CONFIG, &buf);
}

HAL_StatusTypeDef KB_Init(void) {
	uint8_t buf = 0;

	if (PCA9538_Write_Register(KBRD_ADDR, POLARITY_INVERSION, &buf) != HAL_OK) {
		return HAL_ERROR;
	}
	// Выходной регистр держим в 0: строка, переключённая в выход, выдаёт 0
	if (PCA9538_Write_Register(KBRD_ADDR, OUTPUT_PORT, &buf) != HAL_OK) {
		return HAL_ERROR;
	}
	scan_code = 0;
	return KB_SelectRow(0);
}

// Читает выбранную строку и выбирает следующую. После последней строки
// кладёт в *result итог прохода (0 - ничего, 1..12 - одна кнопка,
// KB_MULTI - несколько) и возвращает true
static bool KB_ScanStep(uint8_t *result) {
	uint8_t buf;
	bool done = false;

	if (PCA9538_Read_Inputs(KBRD_ADDR, &buf) != HAL_OK) {
		buf = 0xFF; // ошибка чтения - считаем, что в строке ничего не нажато
	}
	for (uint8_t c = 0; c < sizeof(cols); c++) {
		if (!(buf & cols[c])) {
			scan_code = scan_code ? KB_MULTI : row * sizeof(cols) + c + 1;
		}
	}

	if (row + 1 == sizeof(rows)) {
		*result = scan_code;
		scan_code = 0;
		done = true;
	}
	KB_SelectRow((row + 1) % sizeof(rows));
	return done;
}

uint8_t KB_Poll(void) {
	static uint32_t last_step;
	static uint8_t candidate, count; // что видим сейчас и сколько проходов подряд
	static uint8_t stable;           // состояние после антидребезга
	static bool released = true;     // все кнопки отпущены после прошлого нажатия

	uint32_t now = HAL_GetTick();
	if (now - last_step < KB_ROW_PERIOD_MS) {
		return 0;
	}
	last_step = now;

	uint8_t raw;
	if (!KB_ScanStep(&raw)) {
		return 0;
	}

	if (raw == candidate) {
		if (count < KB_DEBOUNCE_SCANS) {
			count++;
		}
	} else {
		candidate = raw;
		count = 1;
	}
	if (count < KB_DEBOUNCE_SCANS || candidate == stable) {
		return 0;
	}

	stable = candidate;
	if (stable == 0) {
		released = true;
		return 0;
	}
	// Несколько кнопок - считаем, что ничего не нажато.
	// Новое нажатие засчитываем только после того, как отпустили всё
	if (stable == KB_MULTI || !released) {
		return 0;
	}
	released = false;
	return stable;
}
