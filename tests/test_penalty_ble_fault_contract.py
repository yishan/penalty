"""Development hooks use the real transport and are excluded by default."""
from pathlib import Path
import unittest
from test_deep_sleep_contract import function_body

ROOT=Path(__file__).resolve().parents[1]

class FaultContract(unittest.TestCase):
    def test_variant_is_opt_in_and_separate(self):
        kconfig=(ROOT/'main/Kconfig.projbuild').read_text()
        defaults=(ROOT/'sdkconfig.defaults').read_text()
        overlay=(ROOT/'sdkconfig.dev-ble-faults').read_text()
        gate=(ROOT/'tools/validate.sh').read_text()
        self.assertIn('default n',kconfig)
        self.assertNotIn('CONFIG_PENALTY_DEV_BLE_FAULTS=y',defaults)
        self.assertIn('CONFIG_PENALTY_DEV_BLE_FAULTS=y',overlay)
        self.assertIn('--dev-ble-faults',gate)
        self.assertIn('build/dev-ble-faults',gate)
        self.assertIn('development BLE fault hooks enabled in normal build',gate)

    def test_queue_api_and_owner_checks(self):
        source=(ROOT/'main/penalty_ble_session.c').read_text()
        api=function_body(source,'penalty_ble_session_dev_disconnect')
        self.assertIn('send_command(&event)',api)
        self.assertNotIn('ble_gap_terminate',api)
        owner=function_body(source,'handle_internal')
        injection=owner[owner.index('case INTERNAL_DEV_DISCONNECT:'):]
        for guard in ('!s_m4','!s_peer_pinned','!s_secure','s_match_ended','s_end_requested',
                      'PENALTY_BLE_LINK_READY','SESSION_INVALID_HANDLE','s_connection_generation'):
            self.assertIn(guard,injection)
        self.assertIn('ble_gap_terminate(s_conn_handle',injection)
        self.assertNotIn('recover_link(',injection)
        self.assertNotIn('publish_event(',injection)
        self.assertNotIn('penalty_mp_pause(',source)
        self.assertIn('if (s_dev_fault.active) return',function_body(source,'resume_discovery'))
        for name in ('fail_session','penalty_ble_session_start','penalty_ble_session_stop'):
            self.assertIn('penalty_ble_fault_reset(&s_dev_fault)',function_body(source,name))

    def test_menu_does_not_change_match_and_navigation_survives(self):
        source=(ROOT/'main/penalty_multiplayer.c').read_text()
        api=function_body(source,'dev_test_input')
        self.assertNotIn('penalty_mp_input(',api)
        self.assertNotIn('penalty_mp_pause(',api)
        self.assertIn('BSP_BTN_LONG',api)
        self.assertIn('dev_ok_pending',api) # OK LONG must exit, not inject first.
        self.assertIn('penalty_ble_session_dev_disconnect(seconds)',api)
        availability=function_body(source,'dev_test_available')
        for phase in ('PENALTY_MP_AIM','PENALTY_MP_WAIT','PENALTY_MP_RESULT'):
            self.assertIn(phase,availability)
        self.assertNotIn('PENALTY_MP_DONE',availability)
        shell=function_body((ROOT/'main/main.c').read_text(),'input_task')
        self.assertLess(shell.index('launcher_contract_return_to_factory()'),shell.index('penalty_multiplayer_key('))
        self.assertIn('input.btn == BSP_BTN_OK && input.event == BSP_BTN_LONG',shell)

if __name__=='__main__':
    unittest.main()
