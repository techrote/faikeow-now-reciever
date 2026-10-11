<# Read-only inventory. Raw host/device identifiers must stay outside Git. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputPath)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$destination = [IO.Path]::GetFullPath($OutputPath)
if ($destination.StartsWith($repoRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Raw inventory must be written outside the repository.'
}
if (Test-Path -LiteralPath $destination) { throw 'Refusing to overwrite an inventory.' }
$keys = @(
    'DEVPKEY_Device_HardwareIds', 'DEVPKEY_Device_LocationPaths',
    'DEVPKEY_Device_LocationInfo', 'DEVPKEY_Device_Parent',
    'DEVPKEY_Device_BusReportedDeviceDesc', 'DEVPKEY_Device_ProblemCode',
    'DEVPKEY_Device_DriverInfPath', 'DEVPKEY_Device_DriverVersion',
    'DEVPKEY_Device_FirstInstallDate', 'DEVPKEY_Device_LastArrivalDate'
)
$devices = @(Get-PnpDevice -PresentOnly | Where-Object {
    $_.InstanceId -match '^(USB|USBSTOR)\\' -or $_.Class -eq 'Ports'
} | ForEach-Object {
    $device = $_
    $properties = @{}
    Get-PnpDeviceProperty -InstanceId $device.InstanceId -ErrorAction SilentlyContinue |
        Where-Object { $_.KeyName -in $keys } | ForEach-Object {
            $properties[$_.KeyName] = $_.Data
        }
    [ordered]@{
        status = [string]$device.Status; class = [string]$device.Class
        description = $device.FriendlyName; instance_id = $device.InstanceId
        properties = $properties
    }
})
$snapshot = [ordered]@{
    schema_version = 1; captured_at_utc = [DateTime]::UtcNow.ToString('o')
    host_os = (Get-CimInstance Win32_OperatingSystem).Caption
    source_commit = (& git -C $repoRoot rev-parse HEAD)
    source_dirty = [bool](& git -C $repoRoot status --porcelain)
    devices = $devices
    serial_ports = @([IO.Ports.SerialPort]::GetPortNames())
    removable_volumes = @(Get-Volume | Where-Object DriveType -eq Removable |
        Select-Object DriveLetter,FileSystemLabel,FileSystem,Size,UniqueId)
}
$snapshot | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $destination -Encoding utf8
Write-Output "Saved read-only private inventory: $destination"
