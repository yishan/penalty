#include "penalty_preferences.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(penalty_preferences_decode_language(0) == PENALTY_LANGUAGE_EN);
    assert(penalty_preferences_decode_language(1) == PENALTY_LANGUAGE_ZH_CN);
    for (unsigned value = 2; value <= 255; ++value)
        assert(penalty_preferences_decode_language((uint8_t)value) == PENALTY_LANGUAGE_ZH_CN);
    assert(penalty_preferences_encode_language(PENALTY_LANGUAGE_EN) == 0);
    assert(penalty_preferences_encode_language(PENALTY_LANGUAGE_ZH_CN) == 1);
    assert(penalty_preferences_encode_language((penalty_language_t)99) == 1);
    puts("Penalty preferences: PASS");
    return 0;
}
