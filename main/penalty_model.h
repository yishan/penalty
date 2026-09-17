#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PENALTY_SHOTS 5
#define PENALTY_TIMEOUT_MS 4000
#define PENALTY_FLIGHT_MS 600
#define PENALTY_RESULT_MS 900

typedef enum {
    PENALTY_ORIENT, PENALTY_TITLE, PENALTY_SETTINGS, PENALTY_AIM, PENALTY_CHARGE,
    PENALTY_FLIGHT, PENALTY_RESULT, PENALTY_SUMMARY, PENALTY_HELP, PENALTY_EXITING
} penalty_state_t;
/* Clockwise target order. RIGHT advances and LEFT reverses around this ring. */
typedef enum {
    PENALTY_TOP_LEFT,
    PENALTY_TOP_CENTER,
    PENALTY_TOP_RIGHT,
    PENALTY_BOTTOM_RIGHT,
    PENALTY_BOTTOM_CENTER,
    PENALTY_BOTTOM_LEFT,
    PENALTY_TARGET_COUNT
} penalty_direction_t;
typedef enum { PENALTY_EASY, PENALTY_NORMAL, PENALTY_HARD } penalty_difficulty_t;
typedef enum {
    PENALTY_LANGUAGE_EN, PENALTY_LANGUAGE_ZH_CN, PENALTY_LANGUAGE_COUNT
} penalty_language_t;
typedef enum {
    PENALTY_GOAL, PENALTY_PERFECT, PENALTY_SAVE, PENALTY_WEAK,
    PENALTY_HIGH, PENALTY_TIMEOUT, PENALTY_GREEN_GOAL, PENALTY_GREEN_SAVE,
    PENALTY_PERFECT_SAVE, PENALTY_OUTCOME_COUNT
} penalty_outcome_t;
typedef enum {
    PENALTY_INPUT_LEFT, PENALTY_INPUT_RIGHT, PENALTY_INPUT_OK, PENALTY_INPUT_EXIT
} penalty_input_t;

typedef struct {
    penalty_direction_t direction;
    penalty_direction_t keeper;
    uint8_t power;
    uint8_t green_low, green_high; /* Inclusive power values, fixed for this kick. */
    uint8_t perfect_low; /* Two values: perfect_low and perfect_low + 1. */
    uint8_t outcome_roll; /* 0..99; compared with the calibrated goal chance. */
    penalty_outcome_t outcome;
} penalty_shot_t;

typedef struct {
    penalty_state_t state;
    uint64_t since_ms;
    uint64_t last_input_ms;
    uint32_t random;
    uint8_t completed;
    uint8_t goals;
    uint8_t perfect;
    uint8_t selection;
    penalty_difficulty_t difficulty;
    penalty_language_t language;
    bool muted;
    penalty_shot_t current;
    penalty_shot_t shots[PENALTY_SHOTS];
} penalty_model_t;

void penalty_model_init(penalty_model_t *model, uint32_t seed, uint64_t now_ms);
void penalty_model_set_language(penalty_model_t *model, penalty_language_t language);
void penalty_model_tick(penalty_model_t *model, uint64_t now_ms);
/* Only normalized, distinct physical presses enter the model. Timestamps are
 * captured at the button source, not when a queued event is finally handled. */
void penalty_model_input(penalty_model_t *model, penalty_input_t input, uint64_t at_ms);
/* Lost/late input cancels only an uncommitted kick, never a recorded outcome. */
void penalty_model_resync(penalty_model_t *model, uint64_t now_ms);
uint16_t penalty_meter_half_ms(const penalty_shot_t *shot);
uint8_t penalty_power_at(const penalty_shot_t *shot, uint64_t elapsed_ms);
penalty_outcome_t penalty_judge(const penalty_shot_t *shot);
uint8_t penalty_target_column(penalty_direction_t target);
uint8_t penalty_target_row(penalty_direction_t target);
const char *penalty_difficulty_text(penalty_difficulty_t difficulty);
bool penalty_is_goal(penalty_outcome_t outcome);
const char *penalty_outcome_text(penalty_outcome_t outcome);
