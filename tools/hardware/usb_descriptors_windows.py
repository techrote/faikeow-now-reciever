"""Read device/configuration descriptors from one mapped Windows USB hub port.

Only IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION is issued. No reset, suspend,
port cycling or driver change. API layout follows Microsoft's USBView and WDK:
https://learn.microsoft.com/windows-hardware/drivers/ddi/usbioctl/ns-usbioctl-_usb_descriptor_request
The hub API forces device recipient; HID report descriptors are outside its scope.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
import pathlib
import re
import struct
import subprocess
import uuid
from datetime import datetime, timezone
from rp2040_backup import private_destination


def split_configuration(data):
    if len(data) < 9 or data[:2] != b'\x09\x02' or struct.unpack_from('<H', data, 2)[0] != len(data):
        raise ValueError('Incomplete configuration descriptor')
    parts = []; offset = 0
    while offset < len(data):
        size = data[offset]
        if size < 2 or offset + size > len(data):
            raise ValueError('Malformed descriptor length')
        parts.append({'type':data[offset + 1], 'hex':data[offset:offset + size].hex()})
        offset += size
    return parts


def probe(hub_instance, port, expected_vid, expected_pid):
    class GUID(c.Structure):
        _fields_ = [('raw', c.c_ubyte * 16)]
    class Interface(c.Structure):
        _fields_ = [('size', w.DWORD), ('guid', GUID), ('flags', w.DWORD), ('reserved', c.c_size_t)]
    class Device(c.Structure):
        _fields_ = [('size', w.DWORD), ('guid', GUID), ('devinst', w.DWORD), ('reserved', c.c_size_t)]
    setup = c.WinDLL('setupapi', use_last_error=True)
    cfg = c.WinDLL('cfgmgr32', use_last_error=True)
    kernel = c.WinDLL('kernel32', use_last_error=True)
    setup.SetupDiGetClassDevsW.argtypes = [c.POINTER(GUID),w.LPCWSTR,w.HWND,w.DWORD]
    setup.SetupDiGetClassDevsW.restype = w.HANDLE
    setup.SetupDiEnumDeviceInterfaces.argtypes = [w.HANDLE,c.c_void_p,c.POINTER(GUID),w.DWORD,c.POINTER(Interface)]
    setup.SetupDiGetDeviceInterfaceDetailW.argtypes = [w.HANDLE,c.POINTER(Interface),c.c_void_p,w.DWORD,c.POINTER(w.DWORD),c.POINTER(Device)]
    setup.SetupDiDestroyDeviceInfoList.argtypes = [w.HANDLE]
    cfg.CM_Get_Device_IDW.argtypes = [w.DWORD,w.LPWSTR,w.ULONG,w.ULONG]
    kernel.CreateFileW.argtypes = [w.LPCWSTR,w.DWORD,w.DWORD,c.c_void_p,w.DWORD,w.DWORD,w.HANDLE]
    kernel.CreateFileW.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.DeviceIoControl.argtypes = [w.HANDLE,w.DWORD,c.c_void_p,w.DWORD,c.c_void_p,w.DWORD,c.POINTER(w.DWORD),c.c_void_p]
    guid = GUID(); guid.raw[:] = uuid.UUID('f18a0e88-c30c-11d0-8815-00a0c906bed8').bytes_le
    devices = setup.SetupDiGetClassDevsW(c.byref(guid),None,None,0x12)
    if devices == c.c_void_p(-1).value: raise c.WinError(c.get_last_error())
    paths = []
    try:
        for index in range(256):
            interface = Interface(); interface.size = c.sizeof(interface)
            if not setup.SetupDiEnumDeviceInterfaces(devices,None,c.byref(guid),index,c.byref(interface)):
                if c.get_last_error() == 259: break
                raise c.WinError(c.get_last_error())
            required = w.DWORD()
            setup.SetupDiGetDeviceInterfaceDetailW(devices,c.byref(interface),None,0,c.byref(required),None)
            if required.value > 65536: raise ValueError('Unbounded interface path')
            detail = c.create_string_buffer(required.value)
            c.cast(detail,c.POINTER(w.DWORD))[0] = 8 if c.sizeof(c.c_void_p)==8 else 6
            device = Device(); device.size = c.sizeof(device)
            if not setup.SetupDiGetDeviceInterfaceDetailW(devices,c.byref(interface),detail,required,None,c.byref(device)):
                raise c.WinError(c.get_last_error())
            instance = c.create_unicode_buffer(1024)
            if cfg.CM_Get_Device_IDW(device.devinst,instance,len(instance),0): raise ValueError('Cannot identify hub')
            if instance.value.upper() == hub_instance.upper(): paths.append(c.wstring_at(c.addressof(detail)+4))
    finally:
        setup.SetupDiDestroyDeviceInfoList(devices)
    if len(paths) != 1: raise ValueError('Require exactly one interface for the mapped parent hub')
    # USBView opens the hub GENERIC_WRITE for DeviceIoControl. The sole IOCTL
    # below reads standard descriptors; no write/reset IOCTL is reachable.
    handle = kernel.CreateFileW(paths[0],0x40000000,3,None,3,0,None)
    if handle == c.c_void_p(-1).value: raise c.WinError(c.get_last_error())
    def descriptor(kind, length):
        if not 1 <= length <= 4096: raise ValueError('Descriptor length out of bounds')
        request = struct.pack('<IBBHHH',port,0x80,6,kind<<8,0,length)
        buffer = c.create_string_buffer(request + bytes(length),12+length)
        received = w.DWORD()
        if not kernel.DeviceIoControl(handle,(0x22<<16)|(260<<2),buffer,len(buffer),buffer,len(buffer),c.byref(received),None):
            raise c.WinError(c.get_last_error())
        if received.value != 12 + length: raise ValueError('Short descriptor response')
        return buffer.raw[12:received.value]
    try:
        device = descriptor(1,18)
        if device[:2]!=b'\x12\x01' or struct.unpack_from('<HH',device,8)!=(expected_vid,expected_pid):
            raise ValueError('Live USB VID/PID differs from selected target')
        header = descriptor(2,9)
        config = descriptor(2,struct.unpack_from('<H',header,2)[0])
        parts = split_configuration(config)
        return {'device_descriptor_hex':device.hex(),'configuration_descriptor_hex':config.hex(),'configuration_parts':parts,
                'hid_report_descriptor':'BLOCKED by this hub API: device recipient only; use parsed HID caps separately'}
    finally:
        kernel.CloseHandle(handle)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A','B'],required=True)
    parser.add_argument('--identities',type=pathlib.Path,required=True)
    parser.add_argument('--vid',type=lambda s:int(s,0),required=True)
    parser.add_argument('--pid',type=lambda s:int(s,0),required=True)
    parser.add_argument('--output',type=pathlib.Path,required=True)
    args = parser.parse_args(); repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output,repo); destination.mkdir(parents=True)
    inventory = destination/'inventory.json'
    subprocess.run(['pwsh','-NoProfile','-File',str(repo/'tools/hardware/discover_windows.ps1'),'-OutputPath',str(inventory)],check=True)
    initial = json.loads(inventory.read_text(encoding='utf-8-sig'))
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    prefix = f'USB\\VID_{args.vid:04X}&PID_{args.pid:04X}\\'
    targets = [d for d in initial['devices'] if d['instance_id'].upper().startswith(prefix)
               and identity['location_path'] in d['properties'].get('DEVPKEY_Device_LocationPaths',[])]
    if len(targets)!=1: raise ValueError('Require one explicit USB VID/PID at the mapped topology')
    port = re.search(r'#USB\((\d+)\)$',identity['location_path'])
    if not port: raise ValueError('Cannot derive final downstream port')
    result = {'board':args.board,'captured_at_utc':datetime.now(timezone.utc).isoformat(),
              'source_commit_at_start':initial['source_commit'],'source_dirty_at_start':initial['source_dirty'],
              'vid':args.vid,'pid':args.pid,'command':'Read-only hub device/configuration descriptor requests'}
    try:
        result.update(probe(targets[0]['properties']['DEVPKEY_Device_Parent'],int(port[1]),args.vid,args.pid))
        result['result']='PASS'
    except (OSError,ValueError) as error:
        result.update(result='BLOCKED',error=str(error))
        raise
    finally:
        (destination/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PASS: mapped device/configuration descriptors captured privately')


if __name__=='__main__': main()
