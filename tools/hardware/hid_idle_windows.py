"""Observe one mapped FNR mouse for unsolicited motion without injecting input.

An invisible, process-local Raw Input sink observes only the selected device's
RAWMOUSE payloads. Other input is neither suppressed nor saved. This is a bounded
idle observation, not proof of a neutral HID report or active axis/button behavior.
https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-registerrawinputdevices
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
import pathlib
import subprocess
import time
from datetime import datetime, timezone
from rp2040_backup import private_destination


def mouse_interface_path(hid_path):
    # The same device instance exposes different HID and mouse interface GUIDs.
    # https://learn.microsoft.com/windows-hardware/drivers/install/guid-devinterface-mouse
    hid_suffix = '#{4d1e55b2-f16f-11cf-88cb-001111000030}'
    if not hid_path.lower().endswith(hid_suffix):
        raise ValueError('Require a standard HID interface path')
    return hid_path[:-len(hid_suffix)].lower() + '#{378de44c-56ef-11d1-bc8c-00a0c91405dd}'


def observe(device_path, seconds):
    raw_device_path = mouse_interface_path(device_path)
    user = c.WinDLL('user32',use_last_error=True); kernel = c.WinDLL('kernel32',use_last_error=True)
    callback = c.WINFUNCTYPE(c.c_ssize_t,w.HWND,w.UINT,w.WPARAM,w.LPARAM)
    class Header(c.Structure):
        _fields_ = [('kind',w.DWORD),('size',w.DWORD),('device',w.HANDLE),('parameter',w.WPARAM)]
    class Device(c.Structure):
        _fields_ = [('device',w.HANDLE),('kind',w.DWORD)]
    class Registration(c.Structure):
        _fields_ = [('page',w.USHORT),('usage',w.USHORT),('flags',w.DWORD),('window',w.HWND)]
    class Mouse(c.Structure):
        _fields_ = [('flags',w.USHORT),('buttons',w.ULONG),('raw_buttons',w.ULONG),
                    ('x',w.LONG),('y',w.LONG),('extra',w.ULONG)]
    class WindowClass(c.Structure):
        _fields_ = [('size',w.UINT),('style',w.UINT),('proc',callback),('class_extra',c.c_int),
                    ('window_extra',c.c_int),('instance',w.HANDLE),('icon',w.HANDLE),('cursor',w.HANDLE),
                    ('brush',w.HANDLE),('menu',w.LPCWSTR),('name',w.LPCWSTR),('small_icon',w.HANDLE)]
    user.GetRawInputDeviceList.argtypes = [c.POINTER(Device),c.POINTER(w.UINT),w.UINT]
    user.GetRawInputDeviceInfoW.argtypes = [w.HANDLE,w.UINT,c.c_void_p,c.POINTER(w.UINT)]
    user.GetRawInputData.argtypes = [w.HANDLE,w.UINT,c.c_void_p,c.POINTER(w.UINT),w.UINT]
    for function in (user.GetRawInputDeviceList,user.GetRawInputDeviceInfoW,user.GetRawInputData):
        function.restype = w.UINT
    user.RegisterRawInputDevices.argtypes = [c.POINTER(Registration),w.UINT,w.UINT]
    user.RegisterClassExW.argtypes = [c.POINTER(WindowClass)]
    user.CreateWindowExW.argtypes = [w.DWORD,w.LPCWSTR,w.LPCWSTR,w.DWORD,c.c_int,c.c_int,c.c_int,c.c_int,w.HWND,w.HANDLE,w.HANDLE,c.c_void_p]
    user.CreateWindowExW.restype = w.HWND
    user.DefWindowProcW.argtypes = [w.HWND,w.UINT,w.WPARAM,w.LPARAM]; user.DefWindowProcW.restype = c.c_ssize_t
    user.PeekMessageW.argtypes = [c.POINTER(w.MSG),w.HWND,w.UINT,w.UINT,w.UINT]
    user.TranslateMessage.argtypes = [c.POINTER(w.MSG)]
    user.DispatchMessageW.argtypes = [c.POINTER(w.MSG)]; user.DispatchMessageW.restype = c.c_ssize_t
    user.DestroyWindow.argtypes = [w.HWND]; user.UnregisterClassW.argtypes = [w.LPCWSTR,w.HANDLE]
    kernel.GetModuleHandleW.argtypes = [w.LPCWSTR]; kernel.GetModuleHandleW.restype = w.HANDLE

    def selected_device():
        count = w.UINT()
        if user.GetRawInputDeviceList(None,c.byref(count),c.sizeof(Device)) == 0xffffffff:
            raise c.WinError(c.get_last_error())
        if count.value > 256: raise ValueError('Raw device inventory exceeds bound')
        devices = (Device * count.value)()
        used = user.GetRawInputDeviceList(devices,c.byref(count),c.sizeof(Device))
        if used == 0xffffffff: raise c.WinError(c.get_last_error())
        matches = []
        for device in devices[:used]:
            if device.kind != 0: continue
            length = w.UINT()
            user.GetRawInputDeviceInfoW(device.device,0x20000007,None,c.byref(length))
            if not 0 < length.value <= 4096: raise ValueError('Raw device name exceeds bound')
            name = c.create_unicode_buffer(length.value+1)
            if user.GetRawInputDeviceInfoW(device.device,0x20000007,name,c.byref(length)) == 0xffffffff:
                raise c.WinError(c.get_last_error())
            if name.value.lower() == raw_device_path: matches.append(device.device)
        if len(matches)!=1: raise ValueError('Require exactly one raw mouse matching the mapped HID probe')
        return matches[0]

    selected = selected_device(); events = []; errors = []; removed = False
    beginning = time.monotonic()
    def window_proc(window,message,wp,lp):
        nonlocal removed
        if message == 0xfe and wp == 2 and lp == selected: removed = True
        if message == 0xff:
            try:
                header = Header(); size = w.UINT(c.sizeof(header))
                if user.GetRawInputData(w.HANDLE(lp),0x10000005,c.byref(header),c.byref(size),c.sizeof(Header)) != c.sizeof(Header):
                    raise c.WinError(c.get_last_error())
                # Read the body only after identifying the selected raw device.
                if header.device == selected and header.kind == 0:
                    size = w.UINT(c.sizeof(Header)+c.sizeof(Mouse)); data = c.create_string_buffer(size.value)
                    if user.GetRawInputData(w.HANDLE(lp),0x10000003,data,c.byref(size),c.sizeof(Header)) != len(data):
                        raise c.WinError(c.get_last_error())
                    mouse = Mouse.from_buffer_copy(data.raw[c.sizeof(Header):])
                    if len(events) >= 10000: raise ValueError('Target event capacity exceeded')
                    events.append({'elapsed':time.monotonic()-beginning,'x':mouse.x,'y':mouse.y,
                                   'flags':mouse.flags,'button_flags':mouse.buttons & 65535})
            except (OSError,ValueError) as error:
                if len(errors)<16: errors.append(str(error))
        return user.DefWindowProcW(window,message,wp,lp)
    proc = callback(window_proc); instance = kernel.GetModuleHandleW(None)
    name = 'FNR002IdleObservation'; wc = WindowClass()
    wc.size = c.sizeof(wc); wc.proc = proc; wc.instance = instance; wc.name = name
    if not user.RegisterClassExW(c.byref(wc)): raise c.WinError(c.get_last_error())
    window = user.CreateWindowExW(0,name,name,0,0,0,0,0,w.HWND(-3),None,instance,None)
    if not window: raise c.WinError(c.get_last_error())
    registration = Registration(1,2,0x2100,window)  # INPUTSINK + DEVNOTIFY; no suppression/exclusive flags.
    try:
        if not user.RegisterRawInputDevices(c.byref(registration),1,c.sizeof(registration)):
            raise c.WinError(c.get_last_error())
        beginning = time.monotonic(); message = w.MSG()
        while time.monotonic()-beginning < seconds:
            for _ in range(256):
                if not user.PeekMessageW(c.byref(message),None,0,0,1): break
                user.TranslateMessage(c.byref(message)); user.DispatchMessageW(c.byref(message))
            time.sleep(.005)
        if selected_device()!=selected: removed = True
        elapsed = time.monotonic()-beginning
    finally:
        registration = Registration(1,2,1,None)
        user.RegisterRawInputDevices(c.byref(registration),1,c.sizeof(registration))
        user.DestroyWindow(window); user.UnregisterClassW(name,instance)
    motion = sum(bool(e['x'] or e['y']) for e in events)
    return {'elapsed_seconds':elapsed,'events':events,'motion_events':motion,'device_removed':removed,
            'errors':errors,'result':'FAIL' if motion or removed or errors else 'PASS',
            'scope':'No unsolicited target RAWMOUSE motion observed during this idle interval; neutral report and active controls unverified'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board',choices=['A','B'],required=True)
    parser.add_argument('--identities',type=pathlib.Path,required=True)
    parser.add_argument('--probe-record',type=pathlib.Path,required=True)
    parser.add_argument('--seconds',type=int,choices=[20,60],default=20)
    parser.add_argument('--output',type=pathlib.Path,required=True)
    args = parser.parse_args(); repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output,repo); destination.mkdir(parents=True)
    probe = json.loads(args.probe_record.read_text())
    if probe['board']!=args.board or not probe['mouse_caps_match']: raise ValueError('Require mapped mouse capability probe')
    inventory = destination/'inventory.json'
    subprocess.run(['pwsh','-NoProfile','-File',str(repo/'tools/hardware/discover_windows.ps1'),'-OutputPath',str(inventory)],check=True)
    initial = json.loads(inventory.read_text(encoding='utf-8-sig'))
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    parents = [d for d in initial['devices'] if d['instance_id'].upper().startswith('USB\\VID_CAFE&PID_F003\\')]
    if len(parents)!=1 or identity['location_path'] not in parents[0]['properties'].get('DEVPKEY_Device_LocationPaths',[]):
        raise ValueError('HID parent topology mismatch')
    result = {'board':args.board,'source_commit_at_start':initial['source_commit'],'source_dirty_at_start':initial['source_dirty'],
              'image_sha256':probe['image_sha256'],'requested_seconds':args.seconds}
    result.update(observe(probe['device_path'],args.seconds)); result['captured_at_utc']=datetime.now(timezone.utc).isoformat()
    (destination/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(result['result']+': bounded target-only idle mouse observation')
    if result['result']!='PASS': raise SystemExit(1)


if __name__=='__main__': main()
