"""RF result regressions; synthetic counters are not physical RF evidence."""
import argparse
import unittest

from rf_pair_capture import assess, bounded_integer


class RfAssessmentTests(unittest.TestCase):
    def setUp(self):
        self.peer = '02:00:00:00:00:02'
        self.tx = dict(attempted=100, queued=100, enqueue_fail=0, outstanding=0,
                       tx_ok=0, tx_callback_fail=100, uart_overrun=0, uart_rx_error=0, channel=6)
        self.rx = dict(received=0, accepted=0, checked=0, integrity_errors=0,
                       duplicates=0, invalid_length=0, uart_overrun=0, uart_rx_error=0,
                       rejected_source=0, dropped=0, consumed=0, gaps=0,
                       last_source=self.peer, channel=1)

    def check_case(self, kind='wrong_channel'):
        return assess(self.tx, self.rx, kind, 100, self.peer, 1, 6)

    def test_zero_reception_completed_fail_callbacks_pass_negative_case(self):
        self.assertEqual(self.check_case(), [])

    def test_failed_transmit_callbacks_do_not_erase_observed_reception(self):
        # Original anomaly: six valid receptions despite 100 failed callbacks.
        self.rx.update(received=6, accepted=6, checked=6, consumed=6, gaps=58)
        self.assertIn('unexpected wrong-channel reception', self.check_case())

    def test_any_receive_counter_or_acknowledgement_fails_negative_case(self):
        for key in ('received', 'accepted', 'checked'):
            with self.subTest(key=key):
                self.rx[key] = 1
                self.assertIn('unexpected wrong-channel reception', self.check_case())
                self.rx[key] = 0
        self.tx.update(tx_ok=1, tx_callback_fail=99)
        self.assertIn('unexpected wrong-channel acknowledgement', self.check_case())

    def test_absent_callbacks_and_channel_drift_cannot_pass(self):
        self.tx['tx_callback_fail'] = 99
        self.assertIn('missing TX callbacks', self.check_case())
        self.tx['tx_callback_fail'] = 100
        for sample, wrong in ((self.rx, 6), (self.tx, 1)):
            before = sample['channel']
            sample['channel'] = wrong
            self.assertIn('final SDK channel differs from requested configuration', self.check_case())
            sample['channel'] = before

    def test_silent_receiver_does_not_pass_positive_control(self):
        self.assertIn('RF reception/source mismatch', self.check_case('full_250'))
        self.tx.update(tx_ok=100, tx_callback_fail=0)
        self.rx.update(received=100, accepted=100, checked=100, consumed=100)
        self.assertEqual(self.check_case('full_250'), [])
        self.rx['integrity_errors'] = 1
        self.assertIn('RX integrity/UART errors', self.check_case('full_250'))

    def test_source_rejection_requires_every_packet_counted(self):
        self.rx.update(received=100, rejected_source=100)
        self.assertEqual(self.check_case('reject_source'), [])
        self.rx['rejected_source'] = 99
        self.assertIn('source admission failed', self.check_case('reject_source'))

    def test_numeric_options_have_explicit_bounds(self):
        parse = bounded_integer(1, 14)
        self.assertEqual(parse('1'), 1)
        self.assertEqual(parse('14'), 14)
        for value in ('0', '15', '-1'):
            with self.assertRaises(argparse.ArgumentTypeError):
                parse(value)

    def test_power_comparison_requires_matching_final_requested_ceilings(self):
        def check():
            return assess(self.tx, self.rx, 'wrong_channel', 100, self.peer, 1, 6, 0)
        self.assertIn('final diagnostic power ceiling differs from request', check())
        self.tx['power_limit_qdbm'] = self.rx['power_limit_qdbm'] = 0
        self.assertEqual(check(), [])
        self.tx['power_limit_qdbm'] = 82
        self.assertIn('final diagnostic power ceiling differs from request', check())

    def test_power_option_includes_both_sdk_endpoints(self):
        parse = bounded_integer(0, 82)
        self.assertEqual(parse('0'), 0)
        self.assertEqual(parse('82'), 82)
        for value in ('-1', '83'):
            with self.assertRaises(argparse.ArgumentTypeError):
                parse(value)


if __name__ == '__main__':
    unittest.main()
