#include <assert.h>

#include "demo_navigation.h"

int main(void) {
    demo_navigation_t navigation;
    demo_navigation_init(&navigation, 3);

    demo_nav_result_t result = demo_navigation_handle(
        &navigation, DEMO_NAV_INPUT_UP_CLICK, true);
    assert(result.action == DEMO_NAV_ACTION_REFRESH);
    assert(navigation.selected == 2);

    result = demo_navigation_handle(&navigation, DEMO_NAV_INPUT_OK_CLICK, false);
    assert(result.action == DEMO_NAV_ACTION_NONE);
    assert(navigation.active == -1);

    result = demo_navigation_handle(&navigation, DEMO_NAV_INPUT_DOWN_CLICK, true);
    assert(result.action == DEMO_NAV_ACTION_REFRESH);
    assert(navigation.selected == 0);

    result = demo_navigation_handle(&navigation, DEMO_NAV_INPUT_OK_CLICK, true);
    assert(result.action == DEMO_NAV_ACTION_ENTER);
    assert(result.index == 0);
    assert(navigation.active == 0);

    result = demo_navigation_handle(&navigation, DEMO_NAV_INPUT_OTHER, true);
    assert(result.action == DEMO_NAV_ACTION_FORWARD);
    assert(result.index == 0);

    result = demo_navigation_handle(&navigation, DEMO_NAV_INPUT_OK_LONG, true);
    assert(result.action == DEMO_NAV_ACTION_EXIT);
    assert(navigation.active == 0);

    demo_navigation_complete_exit(&navigation);
    assert(navigation.active == -1);
    // A PRESS-triggered game Exit must not let its later CLICK re-enter the
    // selected game. The same applies to DOUBLE/LONG tails after transitions.
    unsigned epoch = 7, press_epoch = 0;
    assert(demo_navigation_event_epoch(epoch, &press_epoch, true) == epoch);
    ++epoch; // page was exited by that press
    for (unsigned tail = 0; tail < 3; ++tail)
        assert(demo_navigation_event_epoch(epoch, &press_epoch, false) != epoch);
    assert(demo_navigation_event_epoch(epoch, &press_epoch, true) == epoch);
    assert(demo_navigation_event_epoch(epoch, &press_epoch, false) == epoch);
    return 0;
}
