"""Source-level lifecycle contracts; physical input and display need board tests."""
from pathlib import Path
import unittest
from test_deep_sleep_contract import function_body

ROOT = Path(__file__).resolve().parents[1]


class PenaltyShellContract(unittest.TestCase):
    def test_m4_has_ephemeral_keys_and_original_peer_only(self):
        session = (ROOT / 'main/penalty_ble_session.c').read_text()
        defaults = (ROOT / 'sdkconfig.defaults').read_text()
        self.assertIn('CONFIG_BT_NIMBLE_NVS_PERSIST=n', defaults)
        self.assertIn('CONFIG_BT_NIMBLE_MAX_BONDS=1', defaults)
        self.assertIn('ble_addr_cmp(&peer.peer_id_addr,&s_peer_address)', session)
        self.assertIn('remote.boot_instance_id!=s_peer_boot', session)
        self.assertIn('ble_store_util_delete_peer(&s_peer_address)', session)
        self.assertNotIn('ble_store_clear(', session)
        self.assertNotIn('nvs_flash_erase(', session)
        self.assertIn('ble_gap_conn_cancel()', function_body(session, 'fail_session'))
        self.assertIn('ble_gap_conn_cancel()', function_body(session, 'penalty_ble_session_stop'))

    def test_m4_applied_ack_and_frozen_recovery(self):
        mp = (ROOT / 'main/penalty_multiplayer.c').read_text()
        worker = function_body(mp, 'worker')
        self.assertIn('.m4_match = true', mp)
        self.assertNotIn('.m3_loop = true', mp)
        self.assertLess(worker.index('penalty_mp_receive('), worker.index('penalty_ble_session_ack_message('))
        self.assertIn('event.message.epoch,event.message.seq', worker)
        self.assertIn('penalty_mp_pause(&s_core', worker)
        self.assertIn('penalty_mp_link_ready(&s_core', worker)
        self.assertIn('atomic_load(&s_stop) && s_core.phase!=PENALTY_MP_DONE', worker)
        self.assertIn('PENALTY_TEXT_MP_WON', function_body(mp, 'render'))
        self.assertIn('PENALTY_TEXT_MP_RECOVERY_FMT', function_body(mp, 'render'))

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

    def test_up_long_returns_to_launcher_from_any_page_and_logs_every_failure(self):
        source = (ROOT / 'main/main.c').read_text()
        body = function_body(source, 'input_task')
        up_long = 'input.btn == BSP_BTN_UP && input.event == BSP_BTN_LONG'
        self.assertIn(up_long, body)
        self.assertLess(body.index(up_long),
                        body.index('s_on_cover && input.btn == BSP_BTN_OK'))
        self.assertEqual(body.count('launcher_contract_return_to_factory()'), 1)
        self.assertIn('if (err != ESP_OK)', body)
        self.assertNotIn('err != ESP_ERR_INVALID_STATE', body)

    def test_cover_defers_ok_press_and_ok_long_never_returns_to_launcher(self):
        source = (ROOT / 'main/main.c').read_text()
        body = function_body(source, 'input_task')
        self.assertIn('s_on_cover && input.btn == BSP_BTN_OK', body)
        cover = body[body.index('if (s_on_cover && input.btn == BSP_BTN_OK)'):
                     body.index('} else if (input.btn == BSP_BTN_OK && input.event == BSP_BTN_LONG)')]
        # A CLICK is emitted after the double-click window. Replaying PRESS
        # must retain CLICK's timestamp, not manufacture >100 ms-old input.
        self.assertNotIn('input.at_ms =', cover)
        self.assertNotIn('s_cover_ok_pressed_at', source)
        self.assertIn('input.event = BSP_BTN_PRESS', body)
        self.assertIn(
            'else if (input.btn == BSP_BTN_OK && input.event == BSP_BTN_LONG)',
            body)
        self.assertEqual(body.count('return_to_cover();'), 4)

    def test_m2_diagnostic_is_hidden_and_starts_ble_only_after_role_selection(self):
        shell = (ROOT / 'main/main.c').read_text()
        input_task = function_body(shell, 'input_task')
        app_main = function_body(shell, 'app_main')
        diag = (ROOT / 'main/penalty_ble_diag.c').read_text()
        worker = function_body(diag, 'worker_task')
        key = function_body(diag, 'penalty_ble_diag_key')
        self.assertIn('s_on_cover && input.btn == BSP_BTN_DOWN', input_task)
        self.assertIn('input.event == BSP_BTN_LONG', input_task)
        self.assertNotIn('penalty_ble_session_start(', app_main)
        self.assertNotIn('penalty_ble_session_start(', worker)
        self.assertIn('start_session(PENALTY_BLE_LINK_HOST)', worker)
        self.assertIn('start_session(PENALTY_BLE_LINK_GUEST)', worker)
        self.assertIn('state == PENALTY_BLE_LINK_OFF', key)
        self.assertIn('button == BSP_BTN_OK && event == BSP_BTN_CLICK', key)

    def test_m2_diagnostic_stops_worker_before_deleting_ui(self):
        shell = (ROOT / 'main/main.c').read_text()
        body = function_body(shell, 'return_to_cover')
        self.assertLess(body.index('penalty_ble_diag_stop()'),
                        body.index('penalty_ble_diag_exit()'))

    def test_m2_health_check_serializes_ping_before_snapshot(self):
        diag = (ROOT / 'main/penalty_ble_diag.c').read_text()
        start = function_body(diag, 'run_guest_health_check')
        events = function_body(diag, 'process_event')
        self.assertIn('penalty_ble_session_send_ping(token)', start)
        self.assertNotIn('penalty_ble_session_request_snapshot()', start)
        ping_case = events.index('PENALTY_BLE_SESSION_EVENT_PING_REPLY')
        snapshot_request = events.index('penalty_ble_session_request_snapshot()')
        self.assertLess(ping_case, snapshot_request)

    def test_m3_single_player_does_not_start_ble_and_exit_keeps_stop_retryable(self):
        app = (ROOT / 'main/penalty_app.c').read_text()
        shell = (ROOT / 'main/main.c').read_text()
        mp = (ROOT / 'main/penalty_multiplayer.c').read_text()
        self.assertNotIn('penalty_ble_session_start(', app)
        key = function_body(app, 'penalty_app_key')
        self.assertIn('s_shootout_menu', key)
        self.assertIn('s_multiplayer_request', key)
        stop = function_body(mp, 'penalty_multiplayer_stop')
        self.assertLess(stop.index('s_stopped, pdMS_TO_TICKS'),
                        stop.index('penalty_ble_session_stop()'))
        self.assertLess(stop.index('penalty_ble_session_stop()'), stop.index('vQueueDelete'))
        cover = function_body(shell, 'return_to_cover')
        self.assertLess(cover.index('penalty_multiplayer_stop()'),
                        cover.index('penalty_multiplayer_exit()'))
        self.assertLess(cover.index('return;'), cover.index('penalty_multiplayer_exit()'))

    def test_m3_pairing_accepts_only_the_value_actually_rendered(self):
        mp = (ROOT / 'main/penalty_multiplayer.c').read_text()
        render = function_body(mp, 'render')
        worker = function_body(mp, 'worker')
        self.assertIn('s_visible_number, v.numeric', render)
        self.assertIn('input.event == BSP_BTN_CLICK', worker)
        self.assertIn('atomic_load(&s_visible_number) == v.numeric', worker)
        self.assertNotIn('penalty_ble_session_confirm_numeric', function_body(mp, 'penalty_multiplayer_key'))

    def test_m2_host_task_signals_before_it_is_deleted(self):
        # nimble_port_freertos_deinit() deletes the host task, so the task must
        # signal first and the stopper must delete it only after that signal.
        session = (ROOT / 'main/penalty_ble_session.c').read_text()
        host = function_body(session, 'host_task')
        stop = function_body(session, 'penalty_ble_session_stop')
        self.assertNotIn('nimble_port_freertos_deinit()', host)
        self.assertLess(host.index('xSemaphoreGive(s_host_stopped)'),
                        host.index('vTaskSuspend(NULL)'))
        self.assertLess(stop.index('s_host_stopped, pdMS_TO_TICKS'),
                        stop.index('nimble_port_freertos_deinit()'))
        self.assertLess(stop.index('nimble_port_freertos_deinit()'),
                        stop.index('nimble_port_deinit()'))

    def test_m2_session_stop_halts_worker_before_host_and_stays_retryable(self):
        session = (ROOT / 'main/penalty_ble_session.c').read_text()
        stop = function_body(session, 'penalty_ble_session_stop')
        self.assertLess(stop.index('INTERNAL_STOP'), stop.index('nimble_port_stop()'))
        self.assertIn('s_host_stop_requested', stop)
        self.assertLess(stop.index('return ESP_ERR_TIMEOUT'),
                        stop.index('atomic_store(&s_running, false)'))

    def test_m2_diag_stop_reports_ble_stop_failure(self):
        diag = (ROOT / 'main/penalty_ble_diag.c').read_text()
        worker = function_body(diag, 'worker_task')
        stop = function_body(diag, 'penalty_ble_diag_stop')
        self.assertNotIn('penalty_ble_session_stop()', worker)
        self.assertEqual(stop.count('return penalty_ble_session_stop();'), 2)

    def test_m2_critical_events_are_never_evicted_and_number_is_shown(self):
        session = (ROOT / 'main/penalty_ble_session.c').read_text()
        publish = function_body(session, 'publish_event')
        fail = function_body(session, 'fail_session')
        self.assertNotIn('xQueueReceive', publish)
        self.assertIn('s_public_overflow', publish)
        self.assertIn('terminate_connection(', fail)
        key = function_body((ROOT / 'main/penalty_ble_diag.c').read_text(),
                            'penalty_ble_diag_key')
        accept = key.index('DIAG_CMD_ACCEPT_NUMERIC')
        self.assertLess(key.index('s_have_numeric'), accept)

    def test_startup_does_not_mark_launcher_trial_valid(self):
        source = (ROOT / 'main/main.c').read_text()
        body = function_body(source, 'app_main')
        self.assertIn('atomic_store(&s_input_ready, true)', body)
        self.assertNotIn('launcher_contract_mark_valid()', body)

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

    def test_portrait_controls_do_not_enter_landscape(self):
        source = (ROOT / 'main/penalty_app.c').read_text()
        key = function_body(source, 'penalty_app_key')
        self.assertIn('btn == BSP_BTN_UP ? PENALTY_INPUT_UP', key)
        self.assertIn('btn == BSP_BTN_DOWN ? PENALTY_INPUT_DOWN', key)
        self.assertNotIn('bsp_lvgl_set_landscape(true)', key)

    def test_low_memory_error_is_localized(self):
        source = (ROOT / 'main/penalty_app.c').read_text()
        enter = function_body(source, 'penalty_app_enter')
        self.assertIn('PENALTY_TEXT_ERROR_UNAVAILABLE', enter)
        self.assertIn('penalty_font_zh_14', enter)


if __name__ == '__main__':
    unittest.main()
