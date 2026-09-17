#include "penalty_i18n.h"
#include "penalty_copy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned utf8_codepoints(const char *text) {
    unsigned count = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if ((*p & 0xc0) != 0x80) ++count;
    return count;
}

int main(void) {
    assert(strcmp(penalty_text(PENALTY_LANGUAGE_EN, PENALTY_TEXT_TITLE_HEADING),
                  "MAKE IT COUNT.") == 0);
    assert(strcmp(penalty_text(PENALTY_LANGUAGE_ZH_CN, PENALTY_TEXT_TITLE_HEADING),
                  "一球定胜负") == 0);
    for (unsigned language = 0; language < PENALTY_LANGUAGE_COUNT; ++language) {
        for (unsigned id = 0; id < PENALTY_TEXT_COUNT; ++id) {
            const char *text = penalty_text((penalty_language_t)language,
                                            (penalty_text_id_t)id);
            assert(text && *text);
            assert(strlen(text) < 256);
        }
        for (unsigned difficulty = 0; difficulty < 3; ++difficulty)
            assert(*penalty_difficulty_text_localized((penalty_language_t)language,
                                                       (penalty_difficulty_t)difficulty));
        for (unsigned direction = 0; direction < PENALTY_TARGET_COUNT; ++direction)
            assert(*penalty_direction_text((penalty_language_t)language,
                                           (penalty_direction_t)direction));
        for (unsigned outcome = 0; outcome < PENALTY_OUTCOME_COUNT; ++outcome) {
            assert(*penalty_outcome_text_localized((penalty_language_t)language,
                                                    (penalty_outcome_t)outcome));
            assert(*penalty_result_copy((penalty_outcome_t)outcome,
                                        (penalty_language_t)language));
        }
    }

    penalty_model_t model;
    penalty_model_init(&model, 42, 0);
    penalty_model_set_language(&model, PENALTY_LANGUAGE_ZH_CN);
    assert(strcmp(penalty_pre_shot_copy(&model), "深绿线 = 90%") == 0);
    model.difficulty = PENALTY_NORMAL;
    model.completed = 1;
    assert(utf8_codepoints(penalty_pre_shot_copy(&model)) <= 16);
    model.completed = PENALTY_SHOTS - 1;
    assert(strcmp(penalty_pre_shot_copy(&model), "最后一球，别紧张。") == 0);

    puts("Penalty i18n: PASS");
    return 0;
}
