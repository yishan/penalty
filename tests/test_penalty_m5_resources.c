#include "penalty_multiplayer_core.h"
#include <stdio.h>

/* Fixed state must not grow with match length; these are host ABI budgets. */
_Static_assert(sizeof(penalty_model_t) <= 1024, "Single-player model budget");
_Static_assert(sizeof(penalty_mp_core_t) <= 2048, "Multiplayer core budget");
_Static_assert(sizeof(penalty_duel_model_t) <= 768, "Duel model budget");
_Static_assert(sizeof(penalty_ble_message_t) <= 272, "Message budget");

int main(void) {
    printf("Penalty M5 bounded state: PASS (host bytes model=%zu duel=%zu multiplayer=%zu message=%zu; not device heap)\n",
        sizeof(penalty_model_t), sizeof(penalty_duel_model_t),
        sizeof(penalty_mp_core_t), sizeof(penalty_ble_message_t));
    return 0;
}
