#include "penalty_copy.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static penalty_model_t view(penalty_difficulty_t difficulty, uint8_t completed) {
    penalty_model_t model = {0};
    model.difficulty = difficulty;
    model.completed = completed;
    model.current.green_low = 37 + completed * 5;
    model.current.perfect_low = model.current.green_low + 2;
    model.current.outcome_roll = completed + 1;
    model.current.keeper = (penalty_direction_t)(completed % PENALTY_TARGET_COUNT);
    return model;
}

static void teaching_is_limited_to_easy_or_first_kick(void) {
    for (unsigned shot = 0; shot < PENALTY_SHOTS; ++shot) {
        penalty_model_t easy = view(PENALTY_EASY, shot);
        assert(strcmp(penalty_pre_shot_copy(&easy), "DARK LINE = 90%") == 0);
    }
    for (unsigned difficulty = PENALTY_NORMAL; difficulty <= PENALTY_HARD; ++difficulty) {
        penalty_model_t first = view((penalty_difficulty_t)difficulty, 0);
        penalty_model_t later = view((penalty_difficulty_t)difficulty, 1);
        assert(strcmp(penalty_pre_shot_copy(&first), "DARK LINE = 90%") == 0);
        assert(strcmp(penalty_pre_shot_copy(&later), "DARK LINE = 90%") != 0);
    }
}

static void interactive_copy_is_stable_short_and_non_repeating(void) {
    penalty_model_t model = view(PENALTY_NORMAL, 1);
    const char *first = penalty_pre_shot_copy(&model);
    assert(strcmp(first, penalty_pre_shot_copy(&model)) == 0);
    assert(strlen(first) <= 28);

    for (unsigned shot = 2; shot < PENALTY_SHOTS - 1; ++shot) {
        model.shots[shot - 1] = model.current;
        model.completed = shot;
        model.current.green_low += 3;
        model.current.perfect_low += 4;
        model.current.outcome_roll += 2;
        const char *next = penalty_pre_shot_copy(&model);
        assert(strcmp(first, next) != 0);
        assert(strlen(next) <= 28);
        first = next;
    }

    model.completed = PENALTY_SHOTS - 1;
    assert(strcmp(penalty_pre_shot_copy(&model), "LAST KICK. NO PRESSURE.") == 0);
}

static void result_copy_matches_the_outcome(void) {
    struct {
        penalty_outcome_t outcome;
        const char *text;
    } cases[] = {
        {PENALTY_GOAL, "BACK OF THE NET."},
        {PENALTY_PERFECT, "NO CHANCE."},
        {PENALTY_SAVE, "KEEPER SAYS NO."},
        {PENALTY_WEAK, "NEEDS MORE BOOT."},
        {PENALTY_HIGH, "ROW Z HAS THE BALL."},
        {PENALTY_TIMEOUT, "REF CHECKS THE CLOCK."},
        {PENALTY_GREEN_GOAL, "THAT FOUND THE CORNER."},
        {PENALTY_GREEN_SAVE, "READ LIKE A BOOK."},
        {PENALTY_PERFECT_SAVE, "WHAT A SAVE."},
        {PENALTY_KEEPER_CATCH, "SAFE HANDS."},
        {PENALTY_KEEPER_PARRY, "PUSHED IT CLEAR."},
        {PENALTY_KEEPER_WRONG_WAY, "SENT THE WRONG WAY."},
        {PENALTY_KEEPER_READ_MISS, "RIGHT READ. JUST MISSED."},
        {PENALTY_KEEPER_EARLY, "DIVED TOO EARLY."},
        {PENALTY_KEEPER_LATE, "DIVED TOO LATE."},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const char *text = penalty_result_copy(cases[i].outcome, PENALTY_LANGUAGE_EN);
        assert(strcmp(text, cases[i].text) == 0);
        assert(strlen(text) <= 28);
    }
}

static void goalkeeper_prompts_are_role_specific(void) {
    penalty_model_t model = view(PENALTY_EASY, 0);
    model.mode = PENALTY_MODE_KEEPER;
    assert(strcmp(penalty_pre_shot_copy(&model), "READ THE KICKER.") == 0);
    model.difficulty = PENALTY_NORMAL;
    model.completed = PENALTY_SHOTS - 1;
    assert(strcmp(penalty_pre_shot_copy(&model), "LAST SAVE. STAY BIG.") == 0);
}

int main(void) {
    teaching_is_limited_to_easy_or_first_kick();
    interactive_copy_is_stable_short_and_non_repeating();
    result_copy_matches_the_outcome();
    goalkeeper_prompts_are_role_specific();
    return 0;
}
