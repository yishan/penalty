#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PENALTY_SHOTS 5
#define PENALTY_TIMEOUT_MS 4000
#define PENALTY_FLIGHT_MS 600
#define PENALTY_RESULT_MS 900

typedef enum {
    PENALTY_COVER, PENALTY_TITLE, PENALTY_SETTINGS, PENALTY_AIM, PENALTY_CHARGE,
    PENALTY_FLIGHT, PENALTY_RESULT, PENALTY_SUMMARY, PENALTY_HELP, PENALTY_EXITING
} penalty_state_t;
/* Stable target IDs; navigation scans each column vertically, not enum order. */
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

#define PENALTY_DEFAULT_LANGUAGE PENALTY_LANGUAGE_ZH_CN
typedef enum {
    PENALTY_MODE_SHOOTER, PENALTY_MODE_KEEPER, PENALTY_MODE_COUNT
} penalty_mode_t;
typedef enum {
    PENALTY_GOAL, PENALTY_PERFECT, PENALTY_SAVE, PENALTY_WEAK,
    PENALTY_HIGH, PENALTY_TIMEOUT, PENALTY_GREEN_GOAL, PENALTY_GREEN_SAVE,
    PENALTY_PERFECT_SAVE,
    PENALTY_KEEPER_CATCH, PENALTY_KEEPER_PARRY, PENALTY_KEEPER_WRONG_WAY,
    PENALTY_KEEPER_READ_MISS, PENALTY_KEEPER_EARLY, PENALTY_KEEPER_LATE,
    PENALTY_OUTCOME_COUNT
} penalty_outcome_t;
typedef enum {
    PENALTY_INPUT_UP, PENALTY_INPUT_DOWN, PENALTY_INPUT_OK, PENALTY_INPUT_EXIT
} penalty_input_t;

typedef struct {
    union { penalty_direction_t player_target; penalty_direction_t direction; };
    union { penalty_direction_t opponent_target; penalty_direction_t keeper; };
    union { uint8_t timing_value; uint8_t power; };
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
    uint8_t saves;
    uint8_t perfect;
    uint8_t selection;
    penalty_mode_t mode;
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
penalty_outcome_t penalty_judge_keeper(const penalty_shot_t *shot);
uint16_t penalty_keeper_cue_ms(penalty_difficulty_t difficulty);
bool penalty_keeper_cue_visible(const penalty_model_t *model, uint64_t now_ms);
uint8_t penalty_target_column(penalty_direction_t target);
uint8_t penalty_target_row(penalty_direction_t target);
const char *penalty_difficulty_text(penalty_difficulty_t difficulty);
bool penalty_is_goal(penalty_outcome_t outcome);
bool penalty_is_save(penalty_outcome_t outcome);
const char *penalty_outcome_text(penalty_outcome_t outcome);
