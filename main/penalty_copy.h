#pragma once

#include "penalty_model.h"

/* Short localized copy sized for the 256 px footer labels. */
const char *penalty_pre_shot_copy(const penalty_model_t *model);
const char *penalty_result_copy(penalty_outcome_t outcome, penalty_language_t language);
