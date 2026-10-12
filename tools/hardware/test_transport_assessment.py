"""Synthetic FNR-005 assessment/guard regressions; never physical evidence."""
import copy
import json
import pathlib
import subprocess
import tempfile
import unittest
from transport_assessment import ESP_FIELDS, PARSER_ERRORS, assess_traffic, assess_corruption, esp_values, qualification_digest, assess_published_phase, assess_rp_restart
from transport_capture import validate_plan, validate_selected_cdc, validate_firmware_source


class AssessmentTests(unittest.TestCase):
    def setUp(self):
        parser = dict.fromkeys(PARSER_ERRORS + ['bytes_received', 'frames_valid', 'datagrams_received',
            'status_received', 'datagrams_replaced', 'datagrams_dropped', 'datagrams_consumed',
            'status_consumed', 'status_dropped', 'status_replaced', 'empty_delimiters'], 0)
        self.before = dict.fromkeys(['delivered', 'statuses', 'pattern_errors', 'metadata_errors', 'duplicates',
            'backwards', 'commands_bad', 'gaps', 'payload_bytes', 'framing', 'parity', 'breaks', 'overruns',
            'datagram_pending', 'status_pending', 'encoded_pending', 'ring_pending'], 0)
        self.before.update(parser=parser, uart=dict.fromkeys(['received', 'popped', 'dropped', 'overflows', 'faults'], 0), digest='0000000000000007', generation='0000000000000000', sequence=0)
        self.after = copy.deepcopy(self.before)
        self.e0 = dict(event='esp', values=[0] * len(ESP_FIELDS), active_us=0, source="a" * 40)
        self.e1 = copy.deepcopy(self.e0)
        values = dict.fromkeys(ESP_FIELDS, 0)
        values.update(generated=100, received=100, accepted=100, ingress_consumed=100, offered=101,
            sent_data=100, sent_status=1, frames_started=102, frames_completed=102,
            bytes_written=27870, hash_lo=7)
        self.e1.update(values=[values[k] for k in ESP_FIELDS], active_us=600000000)
        self.after.update(delivered=100, statuses=1, payload_bytes=25000, sequence=102)
        self.after['parser'].update(bytes_received=27870, frames_valid=102, datagrams_received=101,
            status_received=1, datagrams_consumed=101, status_consumed=1)
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
        self.after.update(delivered=80, payload_bytes=20000, gaps=20, sequence=82)
        self.after['parser'].update(datagrams_received=81, datagrams_consumed=81, frames_valid=82, bytes_received=22350)
        self.after['uart'].update(received=22350, popped=22350)
        self.assertEqual(self.assess(mode='tx_overload'), [])
        self.after['parser']['bad_crc'] = 1
        self.assertTrue(self.assess(mode='tx_overload'))

    def test_consumer_delay_has_separate_drop_policy(self):
        self.after.update(delivered=80, payload_bytes=20000, gaps=20, digest=f'{qualification_digest(20, 80, 250, 7):016x}')
        self.after['parser'].update(datagrams_replaced=20, datagrams_dropped=20, datagrams_consumed=81)
        self.assertEqual(self.assess(mode='consumer_delay'), [])
        self.assertTrue(self.assess())
        self.after['parser']['datagrams_dropped'] = 21
        self.assertTrue(self.assess(mode='consumer_delay'))

    def test_generation_sequence_and_source_must_match_actual_phase(self):
        self.after['generation'] = '0000000000000001'
        self.assertTrue(self.assess())
        self.after['generation'] = '0000000000000000'
        self.after['sequence'] += 1
        self.assertTrue(self.assess())
        self.after['sequence'] -= 1
        self.e1['source'] = 'b' * 40
        self.assertTrue(self.assess())

    def test_initial_pending_state_and_stale_suffix_are_rejected(self):
        self.before['ring_pending'] = 1
        self.assertTrue(self.assess())
        self.before['ring_pending'] = 0
        self.after.update(delivered=80, payload_bytes=20000, digest=f'{qualification_digest(0, 80, 250, 7):016x}')
        self.after['parser'].update(datagrams_replaced=20, datagrams_dropped=20, datagrams_consumed=81)
        self.assertIn('consumer delivery is not exactly the newest suffix', self.assess(mode='consumer_delay'))

    def test_aborts_cannot_be_counted_as_receiver_loss(self):
        self.e1['values'][ESP_FIELDS.index('aborted')] = 1
        self.assertTrue(self.assess(mode='tx_overload'))

    def test_resets_saturation_and_incomplete_telemetry_fail_closed(self):
        self.before['delivered'] = 101
        with self.assertRaises(ValueError): self.assess()
        self.before['delivered'] = 0; self.after['delivered'] = 0xffffffff
        with self.assertRaises(ValueError): self.assess()
        with self.assertRaises(ValueError): esp_values({'event': 'esp', 'values': [0]})

    def test_corruption_requires_rejection_and_zero_escaped_payloads(self):
        after = copy.deepcopy(self.before)
        after['parser'].update(bad_crc=1, frames_valid=1)
        self.assertEqual(assess_corruption(self.before, after, expected_rejections=1), [])
        after['delivered'] = 1
        self.assertIn('invalid payload escaped', assess_corruption(self.before, after, expected_rejections=1))
        after['delivered'] = 0; after['parser']['bad_crc'] = 0
        self.assertTrue(assess_corruption(self.before, after, expected_rejections=1))
        after['parser']['oversized_frames'] = 1
        self.assertEqual(assess_corruption(self.before, after, expected_rejections=1, oversized=True), [])
        self.assertTrue(assess_corruption(self.before, after, expected_rejections=1, empty=True))


class GuardTests(unittest.TestCase):
    def test_later_evidence_commit_preserves_actual_firmware_source(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = pathlib.Path(directory)
            def git(*args):
                return subprocess.check_output(['git', '-C', str(repo), *args], stderr=subprocess.DEVNULL, text=True).strip()
            def commit():
                git('add', '.')
                git('-c', 'user.name=Guard test', '-c', 'user.email=guard@example.invalid', 'commit', '-qm', 'fixture')
                return git('rev-parse', 'HEAD')
            git('init', '-q')
            (repo / 'shared').mkdir()
            (repo / 'shared/fixture.c').write_text('int fixture;\n', encoding='utf-8')
            source = commit()
            tree = validate_firmware_source(repo, source)
            (repo / 'evidence.md').write_text('Evidence-only update\n', encoding='utf-8')
            with self.assertRaises(ValueError): validate_firmware_source(repo, source)
            commit()
            self.assertEqual(validate_firmware_source(repo, source), tree)
            (repo / 'shared/fixture.c').write_text('int changed;\n', encoding='utf-8')
            commit()
            with self.assertRaises(ValueError): validate_firmware_source(repo, source)
            with self.assertRaises(ValueError): validate_firmware_source(repo, 'short')
            with self.assertRaises(ValueError): validate_firmware_source(repo, 'f' * 40)

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


class PublishedEvidenceTests(unittest.TestCase):
    """Replay real captured boundaries; mutations are synthetic negative tests."""
    @classmethod
    def setUpClass(cls):
        path = pathlib.Path(__file__).resolve().parents[2] / 'evidence/FNR-005/physical-results.json'
        cls.phases = json.loads(path.read_text(encoding='utf-8'))

    def test_all_published_observations_reassess(self):
        self.assertGreaterEqual(len(self.phases), 44)
        for phase in self.phases:
            with self.subTest(case=phase['case_id']):
                self.assertEqual(assess_published_phase(phase), [])

    def test_every_phase_rejects_escaped_invalid_or_duplicate_data(self):
        for phase in self.phases:
            bad = copy.deepcopy(phase)
            bad['observations']['rp_after']['duplicates'] += 1
            with self.subTest(case=phase['case_id']):
                self.assertTrue(assess_published_phase(bad))

    def test_loss_boundaries_require_events_and_fresh_payloads(self):
        for phase in self.phases:
            if phase['test_kind'] not in ('ring-overflow', 'uart-overrun', 'consumer-timeout',
                                           'foreground-stall-small', 'foreground-stall-repeat', 'esp-lifecycle',
                                           'esp-cpu-reboot', 'rp-restart'):
                continue
            for field, value in (('events', 0), ('digest', '0000000000000000')):
                if field == 'digest' and phase['test_kind'] in ('consumer-timeout', 'foreground-stall-small', 'foreground-stall-repeat'):
                    continue
                bad = copy.deepcopy(phase)
                bad['observations']['rp_after'][field] = value
                with self.subTest(case=phase['case_id'], field=field):
                    self.assertTrue(assess_published_phase(bad))

    def test_header_fault_must_reject_for_the_expected_reason(self):
        for phase in self.phases:
            if phase['test_kind'] != 'corruption-7': continue
            bad = copy.deepcopy(phase)
            bad['observations']['rp_after']['parser']['bad_version'] -= 1
            bad['observations']['rp_after']['parser']['bad_crc'] += 1
            self.assertIn('wrong production parser rejection reason', assess_published_phase(bad))

    def test_cpu_reboot_does_not_excuse_post_boundary_corruption(self):
        for phase in self.phases:
            if phase['test_kind'] != 'esp-cpu-reboot': continue
            bad = copy.deepcopy(phase)
            bad['observations']['rp_after']['framing'] += 1
            self.assertIn('new post-boundary UART framing', assess_published_phase(bad))


class RpRestartAssessmentTests(unittest.TestCase):
    def setUp(self):
        fixture = AssessmentTests()
        fixture.setUp()
        self.before, self.after, self.e0, self.e1 = fixture.before, fixture.after, fixture.e0, fixture.e1
        self.old = {'datagram_pending': 1}
        self.before['events'] = 1
        self.after.update(events=1, delivered=6, statuses=1, payload_bytes=1500,
                          digest=f'{qualification_digest(600004, 6, 250):016x}')
        self.after['parser'].update(bytes_received=1926, frames_valid=8, datagrams_received=7,
                                   datagrams_consumed=7, status_received=1, status_consumed=1)
        self.after['uart'].update(received=1926, popped=1926)
        v = dict.fromkeys(ESP_FIELDS, 0)
        v.update(generated=10, received=10, accepted=10, ingress_consumed=10, sent_data=10,
                 offered=11, sent_status=1, frames_started=12, frames_completed=12, bytes_written=3030)
        self.e1['values'] = [v[k] for k in ESP_FIELDS]

    def restart(self):
        return assess_rp_restart(self.before, self.after, self.e0, self.e1, self.old,
                                 first=600000, count=10, length=250)

    def test_restart_requires_fresh_suffix_and_new_epoch_accounting(self):
        self.assertEqual(self.restart(), [])
        self.after['digest'] = f'{qualification_digest(600000, 6, 250):016x}'
        self.assertTrue(self.restart())

    def test_restart_rejects_hidden_sender_loss_and_missing_old_ownership(self):
        self.old['datagram_pending'] = 0
        self.assertTrue(self.restart())
        self.old['datagram_pending'] = 1
        self.e1['values'][ESP_FIELDS.index('sent_data')] -= 1
        self.assertTrue(self.restart())

    def test_restart_requires_reset_event_and_no_later_fault(self):
        self.after['events'] = 0
        self.assertTrue(self.restart())
        self.after['events'] = 1
        self.after['overruns'] = 1
        self.assertTrue(self.restart())


if __name__ == '__main__': unittest.main()
