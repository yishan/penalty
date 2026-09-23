#include "penalty_i18n.h"

static const char *const s_text[PENALTY_LANGUAGE_COUNT][PENALTY_TEXT_COUNT] = {
    [PENALTY_LANGUAGE_EN] = {
        "PRESS OK",
        "RETRY: PRESS OK",
        "PENALTY / 01",
        "MAKE IT COUNT.",
        "SHOOTING CHALLENGE",
        "KEEPER CHALLENGE",
        "SETTINGS",
        "HELP",
        "UP/DOWN: SELECT  OK: SET",
        "PENALTY / SETTINGS",
        "SETTINGS",
        "DIFFICULTY: %s",
        "SOUND: %s",
        "LANGUAGE: %s",
        "BACK",
        "UP/DOWN: SELECT  OK: CHANGE",
        "PENALTY / HELP",
        "HOW TO PLAY",
        "UP/DOWN: PICK 6 CELLS\nSHOOT / READ THE KICKER\nOK: LOCK, THEN TIME IT\nDARK 90% / GREEN 65%",
        "OK: BACK TO MENU",
        "HOLD OK: COVER",
        "%s / FULL TIME",
        "%u / 5 GOALS",
        "%u / 5 SAVES",
        "P%u / %s",
        "FLAWLESS",
        "WELL PLAYED",
        "KEEP PRACTISING",
        "RETRY",
        "TITLE",
        "%s %u/5  G:%u",
        "%s %u/5  S:%u",
        "%s  OK: POWER",
        "%s  OK: DIVE",
        "INPUT RESET - AIM AGAIN",
        "OK: SHOOT   POWER %u",
        "OK: DIVE   TIME %u",
        "SHOT AWAY!",
        "DIVE!",
        "RETURNING...",
        "SOUND",
        "MUTED",
        "NO SFX",
        "ENGLISH",
        "Game unavailable\nHold OK to return",
    },
    [PENALTY_LANGUAGE_ZH_CN] = {
        "按 OK 开始",
        "重试：按确认键",
        "点球 / 01",
        "一球定胜负",
        "射门挑战",
        "守门挑战",
        "设置",
        "说明",
        "上下选择  确认进入",
        "点球 / 设置",
        "设置",
        "难度：%s",
        "音效：%s",
        "语言：%s",
        "返回",
        "上下选择  确认修改",
        "点球 / 说明",
        "玩法说明",
        "上下选择六个格子\n射门或守门，观察动作\n确认：锁定，再按时机\n深绿 90% / 绿区 65%",
        "确认键：返回菜单",
        "长按确认：返回封面",
        "%s / 全场结束",
        "%u / 5 进球",
        "%u / 5 扑出",
        "%u 次完美 / %s",
        "完美收官",
        "踢得漂亮",
        "继续练习",
        "重试",
        "标题",
        "%s %u/5  球:%u",
        "%s %u/5  扑:%u",
        "%s  确认：蓄力",
        "%s  确认：锁定扑救",
        "输入已重置，请重新瞄准",
        "确认：射门  力度 %u",
        "确认：扑救  时机 %u",
        "射门！",
        "扑救！",
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
            "CAUGHT!", "PARRIED!", "WRONG WAY", "READ IT - MISSED", "TOO EARLY", "TOO LATE",
        },
        [PENALTY_LANGUAGE_ZH_CN] = {
            "进球！", "完美！", "扑出", "扑出 - 力量不足", "射高", "超时",
            "好球！", "绿区 - 扑出", "完美 - 扑出",
            "接住！", "扑出！", "扑错方向", "判断正确，差一点", "起跳太早", "起跳太晚",
        },
    };
    if ((unsigned)language >= PENALTY_LANGUAGE_COUNT || (unsigned)outcome >= PENALTY_OUTCOME_COUNT) return "";
    return names[language][outcome];
}
