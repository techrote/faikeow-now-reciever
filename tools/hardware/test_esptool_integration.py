"""Pinned esptool object integration; fake serial transport, no hardware IO."""
import unittest
from types import SimpleNamespace
try:
    import esptool
    from esptool.targets.esp8266 import ESP8266StubLoader
except ImportError:
    esptool = None


@unittest.skipIf(esptool is None, 'esptool 4.8.1 installed in the physical-tools environment')
class EsptoolIntegration(unittest.TestCase):
    def test_confirmed_resident_stub_is_reused_without_upload(self):
        self.assertEqual(esptool.__version__, '4.8.1')
        class Serial:
            def flushInput(self):
                pass
            def write(self, data):
                raise AssertionError('Confirmed stub reuse must not upload RAM')
        rom = SimpleNamespace(secure_download_mode=False, _port=Serial(),
                              _trace_enabled=False, cache={})
        stub = ESP8266StubLoader(rom)
        self.assertFalse(stub.sync_stub_detected)
        responses = iter([(0, b'')] * 8)
        stub.command = lambda *args, **kwargs: next(responses)
        stub.sync()  # Exercise pinned SYNC response interpretation, no UART.
        self.assertTrue(stub.sync_stub_detected)
        reused = stub.run_stub()
        self.assertTrue(reused.IS_STUB)
        self.assertIs(reused._port, stub._port)


if __name__ == '__main__':
    unittest.main()
