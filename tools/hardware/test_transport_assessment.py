"""Synthetic FNR-005 assessment/guard regressions; never physical evidence."""
import copy
import unittest
from transport_assessment import ESP_FIELDS, PARSER_ERRORS, assess_traffic, esp_values
from transport_capture import validate_plan, validate_selected_cdc


class AssessmentTests(unittest.TestCase):
    def setUp(self):
        parser = dict.fromkeys(PARSER_ERRORS + ['bytes_received', 'frames_valid', 'datagrams_received',
            'status_received', 'datagrams_replaced', 'datagrams_dropped'], 0)
        self.before = dict.fromkeys(['delivered', 'pattern_errors', 'metadata_errors', 'duplicates',
            'backwards', 'commands_bad', 'gaps', 'payload_bytes', 'framing', 'parity', 'breaks', 'overruns',
            'datagram_pending', 'status_pending', 'encoded_pending', 'ring_pending'], 0)
        self.before.update(parser=parser, uart=dict.fromkeys(['received', 'popped', 'dropped', 'overflows', 'faults'], 0), digest='0000000000000007')
        self.after = copy.deepcopy(self.before)
        self.e0 = dict(event='esp', values=[0] * len(ESP_FIELDS), active_us=0)
        self.e1 = copy.deepcopy(self.e0)
        values = dict.fromkeys(ESP_FIELDS, 0)
        values.update(generated=100, received=100, accepted=100, ingress_consumed=100, offered=101,
            sent_data=100, sent_status=1, frames_started=102, frames_completed=102,
            bytes_written=27870, hash_lo=7)
        self.e1.update(values=[values[k] for k in ESP_FIELDS], active_us=600000000)
        self.after.update(delivered=100, payload_bytes=25000)
        self.after['parser'].update(bytes_received=27870, frames_valid=102, datagrams_received=101, status_received=1)
        self.after['uart'].update(received=27870, popped=27870)

    def assess(self, **kwargs):
        return assess_traffic(self.before, self.after, self.e0, self.e1, count=100, length=250, **kwargs)

    def test_complete_clean_accounting_passes(self):
        self.assertEqual(self.assess(minimum_us=600000000), [])

    def test_every_error_and_overflow_is_rejected(self):
        for section, keys in ((None, ['pattern_errors', 'metadata_errors', 'duplicates', 'backwards',
                'commands_bad', 'framing', 'parity', 'breaks', 'overruns', 'gaps']),
                ('parser', PARSER_ERRORS), ('uart', ['dropped', 'overflows', 'faults'])):
            for key in keys:
                target = self.after[section] if section else self.after
                target[key] += 1
                self.assertTrue(self.assess(), (section, key))
                target[key] -= 1

    def test_each_source_stage_and_wire_byte_is_required(self):
        for key in ['generated', 'received', 'accepted', 'ingress_consumed', 'offered', 'sent_data',
                    'frames_started', 'frames_completed', 'bytes_written', 'hash_lo', 'command_errors']:
            index = ESP_FIELDS.index(key)
            self.e1['values'][index] += 1
            self.assertTrue(self.assess(), key)
            self.e1['values'][index] -= 1
        for section, key in [('parser', 'datagrams_received'), ('parser', 'status_received'),
                             ('parser', 'frames_valid'), ('uart', 'received'), ('uart', 'popped')]:
            self.after[section][key] += 1
            self.assertTrue(self.assess(), (section, key))
            self.after[section][key] -= 1

    def test_short_duration_and_pending_ownership_fail(self):
        self.assertTrue(self.assess(minimum_us=600000001))
        for key in ('datagram_pending', 'status_pending', 'encoded_pending', 'ring_pending'):
            self.after[key] = 1; self.assertTrue(self.assess()); self.after[key] = 0

    def test_overload_is_not_excuse_for_counter_disagreement(self):
        self.assertTrue(self.assess(mode='tx_overload'))
        self.e1['values'][ESP_FIELDS.index('sent_data')] = 80
        self.e1['values'][ESP_FIELDS.index('tx_replaced')] = 20
        self.e1['values'][ESP_FIELDS.index('frames_started')] = 82
        self.e1['values'][ESP_FIELDS.index('frames_completed')] = 82
        self.e1['values'][ESP_FIELDS.index('bytes_written')] = 22350
        self.after.update(delivered=80, payload_bytes=20000, gaps=20)
        self.after['parser'].update(datagrams_received=81, frames_valid=82, bytes_received=22350)
        self.after['uart'].update(received=22350, popped=22350)
        self.assertEqual(self.assess(mode='tx_overload'), [])
        self.after['parser']['bad_crc'] = 1
        self.assertTrue(self.assess(mode='tx_overload'))

    def test_consumer_delay_has_separate_drop_policy(self):
        self.after.update(delivered=80, payload_bytes=20000, gaps=20, digest='0000000000000008')
        self.after['parser'].update(datagrams_replaced=20, datagrams_dropped=20)
        self.assertEqual(self.assess(mode='consumer_delay'), [])
        self.assertTrue(self.assess())
        self.after['parser']['datagrams_dropped'] = 21
        self.assertTrue(self.assess(mode='consumer_delay'))

    def test_aborts_cannot_be_counted_as_receiver_loss(self):
        self.e1['values'][ESP_FIELDS.index('aborted')] = 1
        self.assertTrue(self.assess(mode='tx_overload'))

    def test_resets_saturation_and_incomplete_telemetry_fail_closed(self):
        self.before['delivered'] = 101
        with self.assertRaises(ValueError): self.assess()
        self.before['delivered'] = 0; self.after['delivered'] = 0xffffffff
        with self.assertRaises(ValueError): self.assess()
        with self.assertRaises(ValueError): esp_values({'event': 'esp', 'values': [0]})


class GuardTests(unittest.TestCase):
    def test_bounded_plans(self):
        self.assertEqual(validate_plan([{'command': 'X RUN 18002 250 30 0'}, {'wait': 601}])[1]['wait'], 601)
        for plan in ([], [{'command': 'X RUN 30001 250 30 0'}], [{'wait': 611}],
                     [{'command': 'X RUN 1 251 30 0'}], [{'command': 'F 10001'}],
                     [{'command': 'I 1001'}], [{'command': 'X FAULT 14'}],
                     [{'command': 'X RUN 1 250 30 4294967296'}], [{'command': 'ERASE'}],
                     [{'wait': 1, 'command': 'S'}]):
            with self.assertRaises(ValueError): validate_plan(plan)

    def test_exact_topology_required_among_two_units(self):
        device = {'instance_id': 'USB\\VID_2E8A&PID_000A&MI_00\\x',
                  'description': 'CDC (COM7)',
                  'properties': {'DEVPKEY_Device_LocationPaths': ['mapped#USBMI(0)']}}
        validate_selected_cdc([device], 'COM7', {'location_path': 'mapped'})
        with self.assertRaises(ValueError): validate_selected_cdc([device, device], 'COM7', {'location_path': 'mapped'})
        with self.assertRaises(ValueError): validate_selected_cdc([device], 'COM6', {'location_path': 'mapped'})
        with self.assertRaises(ValueError): validate_selected_cdc([device], 'COM7', {'location_path': 'moved'})


if __name__ == '__main__': unittest.main()
