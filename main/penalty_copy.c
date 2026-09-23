#include "penalty_copy.h"

#include <stddef.h>

static const char *const s_prompts[PENALTY_LANGUAGE_COUNT][8] = {
    [PENALTY_LANGUAGE_EN] = {
        "PICK YOUR MOMENT.", "KEEP YOUR NERVE.", "MAKE IT COUNT.",
        "THE KEEPER'S GUESSING.", "THIS ONE'S FOR THE CROWD.",
        "NO PRESSURE. PROBABLY.", "TRUST YOUR BOOT.", "TOP CORNER? BE BRAVE.",
    },
    [PENALTY_LANGUAGE_ZH_CN] = {
        "看准时机。", "保持冷静。", "这一球要进。", "门将在猜方向。",
        "全场都在看。", "没有压力，大概吧。", "相信你的脚法。", "敢打死角吗？",
    },
};

static const char *const s_keeper_prompts[PENALTY_LANGUAGE_COUNT][8] = {
    [PENALTY_LANGUAGE_EN] = {
        "WATCH THE PLANT FOOT.", "STAY BIG.", "READ THE HIPS.",
        "HOLD YOUR NERVE.", "OWN THE GOAL.", "WAIT FOR THE STRIKE.",
        "TRUST THE READ.", "ONE BIG SAVE.",
    },
    [PENALTY_LANGUAGE_ZH_CN] = {
        "注意支撑脚。", "封住角度。", "观察髋部方向。", "沉住气。",
        "守住球门。", "等他出脚。", "相信判断。", "扑出这一球。",
    },
};

static unsigned raw_prompt_index(const penalty_shot_t *shot, unsigned ordinal) {
    uint32_t mix = (uint32_t)shot->green_low * 3u +
                   (uint32_t)shot->perfect_low * 5u +
                   (uint32_t)shot->outcome_roll * 7u +
                   (uint32_t)shot->keeper * 11u + ordinal * 13u;
    return mix % (sizeof(s_prompts[0]) / sizeof(s_prompts[0][0]));
}

const char *penalty_pre_shot_copy(const penalty_model_t *model) {
    if (!model) return "";
    penalty_language_t language = (unsigned)model->language < PENALTY_LANGUAGE_COUNT
        ? model->language : PENALTY_DEFAULT_LANGUAGE;
    if (model->mode == PENALTY_MODE_KEEPER &&
        (model->difficulty == PENALTY_EASY || model->completed == 0))
        return language == PENALTY_LANGUAGE_ZH_CN ? "观察射手动作。" : "READ THE KICKER.";
    if (model->mode == PENALTY_MODE_SHOOTER &&
        (model->difficulty == PENALTY_EASY || model->completed == 0))
        return language == PENALTY_LANGUAGE_ZH_CN ? "深绿线 = 90%" : "DARK LINE = 90%";
    if (model->mode == PENALTY_MODE_KEEPER && model->completed == PENALTY_SHOTS - 1)
        return language == PENALTY_LANGUAGE_ZH_CN ? "最后一扑，守住球门。" : "LAST SAVE. STAY BIG.";
    if (model->mode == PENALTY_MODE_SHOOTER && model->completed == PENALTY_SHOTS - 1)
        return language == PENALTY_LANGUAGE_ZH_CN ? "最后一球，别紧张。" : "LAST KICK. NO PRESSURE.";

    unsigned index = raw_prompt_index(&model->current, model->completed);
    if (model->completed >= 2) {
        unsigned previous = raw_prompt_index(&model->shots[1], 1);
        for (unsigned shot = 2; shot < model->completed; ++shot) {
            unsigned next = raw_prompt_index(&model->shots[shot], shot);
            if (next == previous) next = (next + 1) % (sizeof(s_prompts[0]) / sizeof(s_prompts[0][0]));
            previous = next;
        }
        if (index == previous) index = (index + 1) % (sizeof(s_prompts[0]) / sizeof(s_prompts[0][0]));
    }
    return model->mode == PENALTY_MODE_KEEPER
        ? s_keeper_prompts[language][index] : s_prompts[language][index];
}

const char *penalty_result_copy(penalty_outcome_t outcome, penalty_language_t language) {
    static const char *const copy[PENALTY_LANGUAGE_COUNT][PENALTY_OUTCOME_COUNT] = {
        [PENALTY_LANGUAGE_EN] = {
            "BACK OF THE NET.", "NO CHANCE.", "KEEPER SAYS NO.", "NEEDS MORE BOOT.",
            "ROW Z HAS THE BALL.", "REF CHECKS THE CLOCK.",
            "THAT FOUND THE CORNER.", "READ LIKE A BOOK.", "WHAT A SAVE.",
            "SAFE HANDS.", "PUSHED IT CLEAR.", "SENT THE WRONG WAY.",
            "RIGHT READ. JUST MISSED.", "DIVED TOO EARLY.", "DIVED TOO LATE.",
        },
        [PENALTY_LANGUAGE_ZH_CN] = {
            "球进了。", "无解。", "门将没给机会。", "再加点力量。",
            "球飞上看台了。", "裁判在看时间。", "擦着死角进了。", "被门将看穿了。", "门将扑出。",
            "稳稳接住。", "奋力扑出。", "被晃开了。", "判断正确，差一点。", "起跳太早。", "起跳太晚。",
        },
    };
    if ((unsigned)language >= PENALTY_LANGUAGE_COUNT || (unsigned)outcome >= PENALTY_OUTCOME_COUNT) return "";
    return copy[language][outcome];
}
