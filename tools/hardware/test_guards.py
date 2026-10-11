"""Safety regression tests: no USB operations or firmware writes."""
import pathlib
import struct
import tempfile
import unittest
import json
import hashlib

from rp2040_backup import private_destination, validate_target
from check_ram_uf2 import validate
from radio_backup import validate_cdc
from radio_flash import validate_backup, validate_image
from link_capture import assess
from rp2040_flash import flash_blocks
from rf_pair_capture import validate_pair, assess as assess_rf


class GuardTests(unittest.TestCase):
    def test_rf_pair_requires_both_labels_ports_and_independent_topologies(self):
        devices = []
        identities = {b: {'location_path': 'USBROOT(0)#USB(' + str(i) + ')'} for b, i in [('A', 1), ('B', 3)]}
        ports = {'A': 'COM6', 'B': 'COM7'}
        for b in ('A', 'B'):
            path = identities[b]['location_path']
            devices += [{'instance_id': 'USB\\VID_2E8A&PID_000A&MI_00\\' + b,
                         'description': 'USB Serial Device (' + ports[b] + ')',
                         'properties': {'DEVPKEY_Device_LocationPaths': [path + '#USBMI(0)']}},
                        {'instance_id': 'USB\\VID_2E8A&PID_000A\\FNR002' + b,
                         'properties': {'DEVPKEY_Device_LocationPaths': [path]}}]
        validate_pair(devices, identities, ports)
        with self.assertRaises(ValueError):
            validate_pair(devices, identities, {'A': 'COM7', 'B': 'COM6'})
        with self.assertRaises(ValueError):
            validate_pair(devices[:-1], identities, ports)

    def test_rf_assessment_separates_integrity_from_deliberate_backpressure(self):
        tx = dict(attempted=100, queued=100, enqueue_fail=0, outstanding=0,
                  tx_ok=100, tx_callback_fail=0, uart_overrun=0, uart_rx_error=0)
        rx = dict(received=100, accepted=100, rejected_source=0, invalid_length=0,
                  checked=100, consumed=100, dropped=0, gaps=0, duplicates=0,
                  integrity_errors=0, uart_overrun=0, uart_rx_error=0, last_source='aa:bb:cc:dd:ee:ff')
        self.assertEqual(assess_rf(tx, rx, 'full_250', 100, rx['last_source']), [])
        rx.update(checked=20, consumed=20, dropped=80, gaps=80)
        self.assertEqual(assess_rf(tx, rx, 'backpressure', 100, rx['last_source']), [])
        self.assertTrue(assess_rf(tx, rx, 'full_250', 100, rx['last_source']))
        rx['integrity_errors'] = 1
        self.assertTrue(assess_rf(tx, rx, 'backpressure', 100, rx['last_source']))

    def test_rf_negative_cases_require_observed_admission_and_channel_outcomes(self):
        tx = dict(attempted=100, queued=100, enqueue_fail=0, outstanding=0,
                  tx_ok=100, tx_callback_fail=0, uart_overrun=0, uart_rx_error=0)
        rx = dict(received=100, accepted=0, rejected_source=100, invalid_length=0,
                  checked=0, consumed=0, dropped=0, gaps=0, duplicates=0,
                  integrity_errors=0, uart_overrun=0, uart_rx_error=0, last_source='00:00:00:00:00:00')
        self.assertEqual(assess_rf(tx, rx, 'reject_source', 100, 'aa:bb:cc:dd:ee:ff'), [])
        self.assertTrue(assess_rf(tx, rx, 'wrong_channel', 100, 'aa:bb:cc:dd:ee:ff'))
        rx.update(received=0, rejected_source=0)
        self.assertEqual(assess_rf(tx, rx, 'wrong_channel', 100, 'aa:bb:cc:dd:ee:ff'), [])

    def test_persistent_uf2_rejects_sram_out_of_capacity_and_aliases(self):
        block = bytearray(512)
        struct.pack_into('<8I', block, 0, 0x0a324655, 0x9e5d5157, 0x2000,
                         0x10000000, 256, 0, 1, 0xe48bff56)
        struct.pack_into('<I', block, 508, 0x0ab16f30)
        self.assertEqual(len(flash_blocks(block, 2097152)), 1)
        for address in (0x20000000, 0x10200000):
            struct.pack_into('<I', block, 12, address)
            with self.assertRaises(ValueError):
                flash_blocks(block, 2097152)
        struct.pack_into('<I', block, 12, 0x10000000)
        struct.pack_into('<I', block, 24, 2)
        second = bytearray(block)
        struct.pack_into('<I', second, 20, 1)
        with self.assertRaises(ValueError):
            flash_blocks(block + second, 2097152)

    def test_link_acceptance_rejects_short_runs_missing_echoes_and_errors(self):
        end = {name: 0 for name in ['gaps', 'pattern_errors', 'crc_errors', 'length_errors',
               'parser_overflows', 'uart_overruns', 'uart_framing', 'uart_parity',
               'uart_breaks', 'ring_drops']}
        end.update(active_duration_us=600000000, sent=12000, echoes=12000, streams=12000,
                   radio_seen=1, radio_id=123, radio_stats_received=1,
                   radio_stats=[12000, 0, 0, 12000, 0, 0, 0, 0, 0])
        self.assertEqual(assess(end, 600, 123), [])
        end['active_duration_us'] = 10000000
        self.assertIn('duration too short', assess(end, 600, 123))
        end['active_duration_us'] = 600000000
        end['echoes'] -= 1
        self.assertIn('missing echoes/timeouts', assess(end, 600, 123))
        end['uart_framing'] = 1
        self.assertIn('uart_framing', assess(end, 600, 123))
        end['radio_stats_received'] = 0
        self.assertIn('radio statistics missing or disagree', assess(end, 600, 123))

    def test_radio_write_requires_both_complete_unchanged_backups(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            data = bytes(1048576)
            digest = hashlib.sha256(data).hexdigest()
            metadata = {'board': 'A', 'matching_independent_reads': True,
                        'size_bytes': 1048576, 'sha256': digest,
                        'reads': [{'mac': 'aa:bb:cc:dd:ee:ff'}] * 2}
            (root / 'backup.json').write_text(json.dumps(metadata))
            for n in (1, 2):
                (root / f'original-{n}.bin').write_bytes(data)
            validate_backup(root, 'A', 'aa:bb:cc:dd:ee:ff')
            with self.assertRaises(ValueError):
                validate_backup(root, 'B', 'aa:bb:cc:dd:ee:ff')
            with self.assertRaises(ValueError):
                validate_backup(root, 'A', 'aa:bb:cc:dd:ee:00')
            image = root / 'original-1.bin'
            validate_image(image, digest, True, metadata)
            (root / 'original-2.bin').write_bytes(data[:-1])
            with self.assertRaises(ValueError):
                validate_backup(root, 'A', 'aa:bb:cc:dd:ee:ff')
            with self.assertRaises(ValueError):
                validate_image(image, 'f' * 64, True, metadata)

    def test_radio_cdc_requires_exact_topology_and_port(self):
        identity = {'location_path': 'USBROOT(0)#USB(1)'}
        device = {'instance_id': 'USB\\VID_2E8A&PID_000A&MI_00\\TEST',
                  'description': 'USB Serial Device (COM5)',
                  'properties': {'DEVPKEY_Device_LocationPaths': ['USBROOT(0)#USB(1)#USBMI(0)']}}
        validate_cdc([device], 'COM5', identity)
        with self.assertRaises(ValueError):
            validate_cdc([device], 'COM6', identity)
        with self.assertRaises(ValueError):
            validate_cdc([device, device], 'COM5', identity)
        with self.assertRaises(ValueError):
            validate_cdc([device], 'COM5', {'location_path': 'USBROOT(0)#USB(3)'})

    def setUp(self):
        self.identity = {'instance_id': 'USB\\VID_2E8A&PID_0003\\ABCD',
                         'location_path': 'USBROOT(0)#USB(1)', 'flash_id': '0123456789ABCDEF'}
        self.device = {'instance_id': self.identity['instance_id'],
                       'properties': {'DEVPKEY_Device_LocationPaths': [self.identity['location_path']]}}

    def test_other_board_on_other_port_is_rejected_despite_same_serial(self):
        self.device['properties']['DEVPKEY_Device_LocationPaths'] = ['USBROOT(0)#USB(3)']
        with self.assertRaises(ValueError):
            validate_target([self.device], self.identity)

    def test_two_targets_with_identical_serial_are_rejected(self):
        with self.assertRaises(ValueError):
            validate_target([self.device, self.device], self.identity)

    def test_missing_target_is_rejected(self):
        with self.assertRaises(ValueError):
            validate_target([], self.identity)

    def test_unique_target_at_explicit_topology_is_selected(self):
        self.assertEqual(validate_target([self.device], self.identity), '0123456789ABCDEF')

    def test_usb_serial_cannot_replace_flash_identity(self):
        self.identity['flash_id'] = 'ABCD'
        with self.assertRaises(ValueError):
            validate_target([self.device], self.identity)

    def test_private_dump_in_repo_or_existing_path_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = pathlib.Path(directory).resolve()
            with self.assertRaises(ValueError):
                private_destination(repo / 'factory.bin', repo)
            with self.assertRaises(ValueError):
                private_destination(repo, repo)
            existing = repo / 'already-present'
            existing.mkdir()
            with self.assertRaises(ValueError):
                private_destination(existing, repo / 'different-repo')

    def test_flash_uf2_is_rejected(self):
        block = bytearray(512)
        struct.pack_into('<8I', block, 0, 0x0a324655, 0x9e5d5157, 0x2000,
                         0x10000000, 256, 0, 1, 0xe48bff56)
        struct.pack_into('<I', block, 508, 0x0ab16f30)
        with self.assertRaises(ValueError):
            validate(block)
        struct.pack_into('<I', block, 12, 0x20000000)
        self.assertEqual(validate(block), 1)
        struct.pack_into('<I', block, 28, 0xe48bff59)
        with self.assertRaises(ValueError):
            validate(block)

    def test_incomplete_or_duplicate_uf2_is_rejected(self):
        with self.assertRaises(ValueError):
            validate(b'')
        block = bytearray(512)
        struct.pack_into('<8I', block, 0, 0x0a324655, 0x9e5d5157, 0x2000,
                         0x20000000, 256, 0, 2, 0xe48bff56)
        struct.pack_into('<I', block, 508, 0x0ab16f30)
        with self.assertRaises(ValueError):
            validate(block + block)


if __name__ == '__main__':
    unittest.main()
