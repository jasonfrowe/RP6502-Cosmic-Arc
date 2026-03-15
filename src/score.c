#include <rp6502.h>
#include <stdint.h>
#include "constants.h"
#include "score.h"

#define SCORE_MAX    999999UL
#define SCORE_DIGITS 6
#define SCORE_COL    17
#define SCORE_ROW    26
#define TILE_DIGIT_0 2

static uint32_t score;

static void score_draw(void)
{
    uint32_t s = score;
    uint8_t digits[SCORE_DIGITS];
    uint8_t i;

    for (i = 0; i < SCORE_DIGITS; i++) {
        digits[SCORE_DIGITS - 1 - i] = (uint8_t)(s % 10);
        s /= 10;
    }

    RIA.addr0 = (unsigned)(MAIN_MAP_TILEMAP_DATA + SCORE_ROW * MAIN_MAP_WIDTH_TILES + SCORE_COL);
    RIA.step0 = 1;
    for (i = 0; i < SCORE_DIGITS; i++) {
        RIA.rw0 = (uint8_t)(TILE_DIGIT_0 + digits[i]);
    }
}

void score_init(void)
{
    score = 0;
    score_draw();
}

void score_add(uint32_t points)
{
    if (score < SCORE_MAX) {
        score += points;
        if (score > SCORE_MAX)
            score = SCORE_MAX;
        score_draw();
    }
}
