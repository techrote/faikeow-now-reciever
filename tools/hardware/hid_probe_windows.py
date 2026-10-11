"""Read Windows HID capabilities and GET_REPORT from one mapped FNR mouse.

Opens the HID collection with zero read/write access. Sends no output report,
synthetic input, device reset, suspend request or configuration change.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
import pathlib
import subprocess
from datetime import datetime, timezone

from rp2040_backup import private_destination


def probe():
    class GUID(c.Structure):
        _fields_ = [('a', w.DWORD), ('b', w.WORD), ('c', w.WORD), ('d', c.c_byte * 8)]
    class Interface(c.Structure):
        _fields_ = [('size', w.DWORD), ('guid', GUID), ('flags', w.DWORD), ('reserved', c.c_size_t)]
    class Attributes(c.Structure):
        _fields_ = [('size', w.DWORD), ('vid', w.WORD), ('pid', w.WORD), ('version', w.WORD)]
    class Caps(c.Structure):
        _fields_ = [(name, w.WORD) for name in ('usage', 'usage_page', 'input_bytes', 'output_bytes', 'feature_bytes')]
        _fields_ += [('reserved', w.WORD * 17)]
        _fields_ += [(name, w.WORD) for name in ('link_nodes', 'input_button_caps', 'input_value_caps',
                     'input_data_indices', 'output_button_caps', 'output_value_caps', 'output_data_indices',
                     'feature_button_caps', 'feature_value_caps', 'feature_data_indices')]
    hid, setup, kernel = c.WinDLL('hid', use_last_error=True), c.WinDLL('setupapi', use_last_error=True), c.WinDLL('kernel32', use_last_error=True)
    hid.HidD_GetHidGuid.argtypes = [c.POINTER(GUID)]
    setup.SetupDiGetClassDevsW.argtypes = [c.POINTER(GUID), w.LPCWSTR, w.HWND, w.DWORD]
    setup.SetupDiGetClassDevsW.restype = w.HANDLE
    setup.SetupDiEnumDeviceInterfaces.argtypes = [w.HANDLE, c.c_void_p, c.POINTER(GUID), w.DWORD, c.POINTER(Interface)]
    setup.SetupDiGetDeviceInterfaceDetailW.argtypes = [w.HANDLE, c.POINTER(Interface), c.c_void_p, w.DWORD, c.POINTER(w.DWORD), c.c_void_p]
    setup.SetupDiDestroyDeviceInfoList.argtypes = [w.HANDLE]
    kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p, w.DWORD, w.DWORD, w.HANDLE]
    kernel.CreateFileW.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    hid.HidD_GetAttributes.argtypes = [w.HANDLE, c.POINTER(Attributes)]
    hid.HidD_GetPreparsedData.argtypes = [w.HANDLE, c.POINTER(c.c_void_p)]
    hid.HidD_FreePreparsedData.argtypes = [c.c_void_p]
    hid.HidP_GetCaps.argtypes = [c.c_void_p, c.POINTER(Caps)]
    hid.HidP_GetCaps.restype = c.c_long
    hid.HidD_GetInputReport.argtypes = [w.HANDLE, c.c_void_p, w.ULONG]
    guid = GUID(); hid.HidD_GetHidGuid(c.byref(guid))
    devices = setup.SetupDiGetClassDevsW(c.byref(guid), None, None, 0x12)
    if devices == c.c_void_p(-1).value:
        raise c.WinError(c.get_last_error())
    found = []
    try:
        for index in range(256):
            interface = Interface(); interface.size = c.sizeof(interface)
            if not setup.SetupDiEnumDeviceInterfaces(devices, None, c.byref(guid), index, c.byref(interface)):
                if c.get_last_error() == 259: break
                raise c.WinError(c.get_last_error())
            required = w.DWORD()
            setup.SetupDiGetDeviceInterfaceDetailW(devices, c.byref(interface), None, 0, c.byref(required), None)
            detail = c.create_string_buffer(required.value)
            c.cast(detail, c.POINTER(w.DWORD))[0] = 8 if c.sizeof(c.c_void_p) == 8 else 6
            if not setup.SetupDiGetDeviceInterfaceDetailW(devices, c.byref(interface), detail, required, None, None):
                raise c.WinError(c.get_last_error())
            path = c.wstring_at(c.addressof(detail) + 4)
            if 'vid_cafe&pid_f003' not in path.lower(): continue
            handle = kernel.CreateFileW(path, 0, 3, None, 3, 0, None)
            if handle == c.c_void_p(-1).value:
                raise c.WinError(c.get_last_error())
            try:
                attrs = Attributes(); attrs.size = c.sizeof(attrs)
                if not hid.HidD_GetAttributes(handle, c.byref(attrs)):
                    raise c.WinError(c.get_last_error())
                prepared = c.c_void_p()
                if not hid.HidD_GetPreparsedData(handle, c.byref(prepared)):
                    raise c.WinError(c.get_last_error())
                try:
                    caps = Caps()
                    if hid.HidP_GetCaps(prepared, c.byref(caps)) != 0x00110000:
                        raise ValueError('HidP_GetCaps failed')
                finally:
                    hid.HidD_FreePreparsedData(prepared)
                report = c.create_string_buffer(caps.input_bytes)
                ok = bool(hid.HidD_GetInputReport(handle, report, caps.input_bytes))
                found.append({'device_path': path, 'vid': attrs.vid, 'pid': attrs.pid,
                              'caps': {name: getattr(caps, name) for name, _ in caps._fields_ if name != 'reserved'},
                              'get_input_report_succeeded': ok,
                              'get_input_report_error': 0 if ok else c.get_last_error(),
                              'input_report_hex': report.raw.hex() if ok else None})
            finally:
                kernel.CloseHandle(handle)
    finally:
        setup.SetupDiDestroyDeviceInfoList(devices)
    if len(found) != 1:
        raise ValueError('Require exactly one FNR HID collection')
    return found[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--write-record', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    write = json.loads(args.write_record.read_text())
    if write['board'] != args.board or not write['exact_readback']:
        raise ValueError('Require exact selected image deployment record')
    destination.mkdir(parents=True)
    inventory = destination / 'inventory.json'
    subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                    '-OutputPath', str(inventory)], check=True)
    devices = json.loads(inventory.read_text(encoding='utf-8-sig'))['devices']
    parents = [d for d in devices if d['instance_id'].upper().startswith('USB\\VID_CAFE&PID_F003\\')]
    if len(parents) != 1 or identity['location_path'] not in parents[0]['properties'].get('DEVPKEY_Device_LocationPaths', []):
        raise ValueError('HID USB parent topology mismatch')
    result = probe()
    result.update(board=args.board, captured_at_utc=datetime.now(timezone.utc).isoformat(),
                  source_commit=subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
                  image_sha256=write['image_sha256'])
    caps = result['caps']
    result['mouse_caps_match'] = (caps['usage_page'] == 1 and caps['usage'] == 2
                                  and caps['input_bytes'] == 4 and caps['input_button_caps'] == 1
                                  and caps['input_value_caps'] == 2 and caps['output_bytes'] == 0
                                  and caps['feature_bytes'] == 0)
    result['neutral_get_report'] = result['get_input_report_succeeded'] and result['input_report_hex'] == '00000000'
    (destination / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print({k: v for k, v in result.items() if k != 'device_path'})


if __name__ == '__main__':
    main()
