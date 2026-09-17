#include "penalty_i18n.h"

static const char *const s_text[PENALTY_LANGUAGE_COUNT][PENALTY_TEXT_COUNT] = {
    [PENALTY_LANGUAGE_EN] = {
        "TURN RIGHT 90°\nTHEN PRESS OK",
        "RETRY: PRESS OK",
        "PENALTY / 01",
        "MAKE IT COUNT.",
        "PLAY",
        "SETTINGS",
        "HELP",
        "< / > SELECT    OK: SET",
        "PENALTY / SETTINGS",
        "SETTINGS",
        "DIFFICULTY: %s",
        "SOUND: %s",
        "LANGUAGE: %s",
        "BACK",
        "< / > SELECT    OK: CHANGE",
        "PENALTY / HELP",
        "HOW TO PLAY",
        "LEFT/RIGHT: CYCLE 6 TARGETS\nOK: charge, then shoot\nDark 90% / Green 65%\nOutside green 25% / 5 kicks",
        "OK: BACK TO MENU",
        "HOLD OK: COVER",
        "%s / FULL TIME",
        "%u / 5 GOALS",
        "%u PERFECT  /  %s",
        "FLAWLESS",
        "WELL PLAYED",
        "KEEP PRACTISING",
        "RETRY",
        "TITLE",
        "%s %u/5  G:%u",
        "< %s >   OK: POWER",
        "INPUT RESET - AIM AGAIN",
        "OK: SHOOT   POWER %u",
        "SHOT AWAY!",
        "RETURNING...",
        "SOUND",
        "MUTED",
        "NO SFX",
        "ENGLISH",
        "Game unavailable\nHold OK to return",
    },
    [PENALTY_LANGUAGE_ZH_CN] = {
        "设备向右横转 90°\n再按确认键",
        "重试：按确认键",
        "点球 / 01",
        "一球定胜负",
        "开始",
        "设置",
        "说明",
        "左右选择  确认进入",
        "点球 / 设置",
        "设置",
        "难度：%s",
        "音效：%s",
        "语言：%s",
        "返回",
        "左右选择  确认修改",
        "点球 / 说明",
        "玩法说明",
        "左右选择：左中右 / 上下\n确认键：蓄力，再按射门\n深绿线：90% / 绿区：65%\n普通力度：25% / 共 5 球",
        "确认键：返回菜单",
        "长按确认：返回封面",
        "%s / 全场结束",
        "%u / 5 进球",
        "%u 次完美 / %s",
        "完美收官",
        "踢得漂亮",
        "继续练习",
        "重试",
        "标题",
        "%s %u/5  球:%u",
        "< %s >  确认：蓄力",
        "输入已重置，请重新瞄准",
        "确认：射门  力度 %u",
        "射门！",
        "返回中...",
        "开启",
        "静音",
        "无音效",
        "中文",
        "游戏不可用\n长按确认返回",
    },
};

const char *penalty_text(penalty_language_t language, penalty_text_id_t id) {
    if ((unsigned)language >= PENALTY_LANGUAGE_COUNT || (unsigned)id >= PENALTY_TEXT_COUNT)
        return "";
    return s_text[language][id];
}

const char *penalty_difficulty_text_localized(penalty_language_t language,
                                               penalty_difficulty_t difficulty) {
    static const char *const names[PENALTY_LANGUAGE_COUNT][3] = {
        [PENALTY_LANGUAGE_EN] = {"EASY", "NORMAL", "HARD"},
        [PENALTY_LANGUAGE_ZH_CN] = {"简单", "普通", "困难"},
    };
    if ((unsigned)language >= PENALTY_LANGUAGE_COUNT || (unsigned)difficulty >= 3) return "";
    return names[language][difficulty];
}

const char *penalty_direction_text(penalty_language_t language,
                                   penalty_direction_t direction) {
    static const char *const names[PENALTY_LANGUAGE_COUNT][PENALTY_TARGET_COUNT] = {
        [PENALTY_LANGUAGE_EN] = {
            "LEFT HIGH", "CENTER HIGH", "RIGHT HIGH",
            "RIGHT LOW", "CENTER LOW", "LEFT LOW",
        },
        [PENALTY_LANGUAGE_ZH_CN] = {"左上", "中上", "右上", "右下", "中下", "左下"},
    };
    if ((unsigned)language >= PENALTY_LANGUAGE_COUNT || (unsigned)direction >= PENALTY_TARGET_COUNT) return "";
    return names[language][direction];
}

const char *penalty_outcome_text_localized(penalty_language_t language,
                                            penalty_outcome_t outcome) {
    static const char *const names[PENALTY_LANGUAGE_COUNT][PENALTY_OUTCOME_COUNT] = {
        [PENALTY_LANGUAGE_EN] = {
            "GOAL!", "PERFECT!", "SAVE", "SAVE - WEAK", "MISS - HIGH", "MISS - TIME",
            "GOOD SHOT!", "GREEN - SAVE", "PERFECT - SAVE",
        },
        [PENALTY_LANGUAGE_ZH_CN] = {
            "进球！", "完美！", "扑出", "扑出 - 力量不足", "射高", "超时",
            "好球！", "绿区 - 扑出", "完美 - 扑出",
        },
    };
    if ((unsigned)language >= PENALTY_LANGUAGE_COUNT || (unsigned)outcome >= PENALTY_OUTCOME_COUNT) return "";
    return names[language][outcome];
}
