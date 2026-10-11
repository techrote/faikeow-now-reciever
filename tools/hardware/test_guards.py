"""Safety regression tests: no USB operations or firmware writes."""
import pathlib
import tempfile
import unittest

from rp2040_backup import private_destination, validate_target


class GuardTests(unittest.TestCase):
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


if __name__ == '__main__':
    unittest.main()
