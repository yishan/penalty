"""M5 integration contracts; runtime resources still require physical tests."""
from pathlib import Path
import unittest
from test_deep_sleep_contract import function_body

ROOT = Path(__file__).resolve().parents[1]


class PenaltyM5Contract(unittest.TestCase):
    def test_complete_gate_runs_both_renderers_and_size_report(self):
        gate = (ROOT / 'tools/validate.sh').read_text()
        preview = (ROOT / 'tools/preview-penalty.sh').read_text()
        self.assertIn('./tools/preview-penalty.sh', gate)
        self.assertIn('"${validation_build_dir}" size', gate)
        self.assertIn('/penalty_preview" "${output_dir}"', preview)
        self.assertIn('/penalty_mp_preview" "${output_dir}"', preview)

    def test_resource_sampling_is_worker_owned_and_stack_units_are_bytes(self):
        ble = (ROOT / 'main/penalty_ble_session.c').read_text()
        mp = (ROOT / 'main/penalty_multiplayer.c').read_text()
        self.assertIn('sample_resources()', function_body(ble, 'worker_task'))
        self.assertNotIn('sample_resources()', function_body(ble, 'gap_event'))
        self.assertNotIn('heap_caps_', function_body(mp, 'penalty_multiplayer_key'))
        self.assertIn('next_sample = now_ms() + 1000', function_body(mp, 'worker'))
        self.assertIn('bytes, queue peak=', ble)
        self.assertNotIn('words, queue peak=', ble)
        self.assertLess(function_body(ble, 'host_task').index('host_stack_high_water'),
                        function_body(ble, 'host_task').index('xSemaphoreGive'))

    def test_teardown_reports_resources_without_starting_ble_for_single_player(self):
        mp = (ROOT / 'main/penalty_multiplayer.c').read_text()
        app = (ROOT / 'main/penalty_app.c').read_text()
        self.assertIn('lv_mem_monitor', function_body(mp, 'penalty_multiplayer_exit'))
        self.assertNotIn('penalty_ble_session_start(', app)
        self.assertNotIn('nimble_port_init(', app)
        self.assertIn('atomic_store(&s_input_drops, 0)', function_body(mp, 'penalty_multiplayer_start'))


if __name__ == '__main__':
    unittest.main()
