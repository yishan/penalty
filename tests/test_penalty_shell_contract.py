"""Source-level lifecycle contracts; physical input and display need board tests."""
from pathlib import Path
import unittest
from test_deep_sleep_contract import function_body

ROOT = Path(__file__).resolve().parents[1]


class PenaltyShellContract(unittest.TestCase):
    def test_startup_opens_game_before_optional_audio(self):
        source = (ROOT / 'main/main.c').read_text()
        body = function_body(source, 'app_main')
        self.assertNotIn('enter_menu()', body)
        self.assertIn('penalty_app_enter()', body)
        self.assertLess(body.index('penalty_app_enter()'), body.index('bsp_audio_init()'))

    def test_shell_has_no_demo_launcher(self):
        source = (ROOT / 'main/main.c').read_text()
        self.assertNotIn('DEMOS[]', source)
        self.assertNotIn('ui_pixel_screen_create("FoloToy")', source)

    def test_cover_defers_ok_press_and_long_returns_to_launcher(self):
        source = (ROOT / 'main/main.c').read_text()
        body = function_body(source, 'input_task')
        self.assertIn('s_on_cover && input.btn == BSP_BTN_OK', body)
        self.assertIn('s_cover_ok_pressed_at = input.at_ms', body)
        self.assertIn('launcher_contract_return_to_factory()', body)
        self.assertIn('input.event = BSP_BTN_PRESS', body)
        self.assertLess(body.index('input.event == BSP_BTN_LONG'),
                        body.index('input.event = BSP_BTN_PRESS'))

    def test_trial_is_confirmed_only_after_input_is_ready(self):
        source = (ROOT / 'main/main.c').read_text()
        body = function_body(source, 'app_main')
        ready = 'atomic_store(&s_input_ready, true)'
        confirm = 'launcher_contract_mark_valid()'
        self.assertIn(ready, body)
        self.assertIn(confirm, body)
        self.assertLess(body.index(ready), body.index(confirm))

    def test_return_stops_producers_before_recreating_cover(self):
        source = (ROOT / 'main/main.c').read_text()
        body = function_body(source, 'return_to_cover')
        calls = ['penalty_app_stop()', 'penalty_app_exit()', 'penalty_app_enter()',
                 'atomic_fetch_add(&s_input_epoch, 1)', 'penalty_app_start()']
        positions = [body.index(call) for call in calls]
        self.assertEqual(positions, sorted(positions))

    def test_audio_format_and_volume_are_both_checked(self):
        source = (ROOT / 'main/penalty_app.c').read_text()
        body = function_body(source, 'service_task')
        format_call = 'bsp_audio_set_format(16000, 16, 1)'
        volume_call = 'bsp_audio_set_volume(PENALTY_AUDIO_VOLUME_PERCENT)'
        self.assertIn(format_call, body)
        self.assertIn(volume_call, body)
        self.assertLess(body.index(format_call), body.index(volume_call))
        self.assertIn('volume_err', body)

    def test_language_is_loaded_before_the_first_cover(self):
        source = (ROOT / 'main/main.c').read_text()
        body = function_body(source, 'app_main')
        initialize = 'penalty_preferences_init()'
        restore = 'penalty_app_set_initial_language(penalty_preferences_load_language())'
        enter = 'penalty_app_enter()'
        self.assertIn(initialize, body)
        self.assertIn(restore, body)
        self.assertLess(body.index(initialize), body.index(restore))
        self.assertLess(body.index(restore), body.index(enter))

    def test_language_writes_run_in_the_worker_and_flush_on_stop(self):
        source = (ROOT / 'main/penalty_app.c').read_text()
        service = function_body(source, 'service_task')
        key = function_body(source, 'penalty_app_key')
        self.assertIn('save_pending_language()', service)
        self.assertGreaterEqual(service.count('save_pending_language()'), 2)
        self.assertIn('before.language != s_model.language', key)
        self.assertIn('atomic_store(&s_language_save_pending, true)', key)

    def test_low_memory_error_is_localized(self):
        source = (ROOT / 'main/penalty_app.c').read_text()
        enter = function_body(source, 'penalty_app_enter')
        self.assertIn('PENALTY_TEXT_ERROR_UNAVAILABLE', enter)
        self.assertIn('penalty_font_zh_14', enter)


if __name__ == '__main__':
    unittest.main()
