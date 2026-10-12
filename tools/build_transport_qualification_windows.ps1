<# Supplemental qualified Windows build lane. Builds only; never accesses hardware. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ManifestPath,
      [Parameter(Mandatory)][string]$PrivateRadioBackup)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location -LiteralPath $repo
$source = (& git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $source.Length -ne 40 -or (& git status --porcelain)) {
    throw 'Require committed clean exact source before qualification builds'
}
$destination = [IO.Path]::GetFullPath($ManifestPath)
if ($destination.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $destination)) {
    throw 'Use a new private manifest outside the repository'
}
$python = (Resolve-Path '.deps/hardware/python/Scripts/python.exe').Path
$cmake = (Resolve-Path '.deps/hardware/python/Scripts/cmake.exe').Path
$arm = (Resolve-Path '.deps/hardware/arm/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi/bin').Path
$cli = (Resolve-Path '.deps/hardware/arduino-cli/arduino-cli.exe').Path
$config = (Resolve-Path '.deps/hardware/arduino-cli/arduino-cli.yaml').Path
$sdk = (Resolve-Path '.deps/pico-sdk').Path
$picotool = (Resolve-Path '.deps/hardware/picotool-2.2.0/picotool').Path
$env:PATH = $arm + ';' + (Split-Path $cmake) + ';' + $env:PATH
$env:PICO_SDK_PATH = $sdk
if ((& git -C $sdk rev-parse HEAD) -ne 'a1438dff1d38bd9c65dbd693f0e5db4b9ae91779' -or
    (& git -C "$sdk/lib/tinyusb" rev-parse HEAD) -ne '86ad6e56c1700e85f1c5678607a762cfe3aa2f47') { throw 'SDK source identity mismatch' }
$commands = [Collections.Generic.List[object]]::new()
function Invoke-Build([string]$Tool, [string[]]$Arguments, [string]$Log) {
    $commands.Add(@{tool=$Tool; arguments=$Arguments; log=$Log})
    & $Tool @Arguments *> $Log
    if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $Log -Tail 25; throw "Build failed: $Log" }
}
$common = @('-G','Ninja','-DPICO_BOARD=pico','-DPICO_PLATFORM=rp2040',"-DPICO_SDK_PATH=$sdk",
    "-Dpicotool_DIR=$picotool",'-DCMAKE_BUILD_TYPE=Release')
Invoke-Build $cmake (@('-S','firmware/rp2040','-B','build/fnr005-production-rp-win') + $common) 'build/fnr005-production-rp-config.log'
Invoke-Build $cmake @('--build','build/fnr005-production-rp-win','--parallel','2') 'build/fnr005-production-rp-build.log'
Invoke-Build $cmake (@('-S','diagnostics/transport-qualification/rp2040','-B','build/fnr005-rp-win',
    '-DPICO_NO_FLASH=1',"-DFNR_SOURCE_ID=$source") + $common) 'build/fnr005-rp-config.log'
Invoke-Build $cmake @('--build','build/fnr005-rp-win','--parallel','2') 'build/fnr005-rp-build.log'
Invoke-Build $python @('tools/hardware/check_ram_uf2.py','build/fnr005-rp-win/fnr_transport_observer.uf2') 'build/fnr005-ram-validation.log'
$root = $repo.Replace('\','/')
$cppFlags = "-I$root/diagnostics/transport-qualification -I$root/shared/include -I$root/shared/src -I$root/firmware/esp8266 -DFNR_SOURCE_ID=$source"
$cFlags = "-I$root/shared/include -I$root/shared/src"
$espCommon = @('--config-file',$config,'compile','--fqbn','esp8266:esp8266:esp8285:xtal=80,CrystalFreq=26,eesz=1M,wipe=none',
    '--build-property',"compiler.cpp.extra_flags=$cppFlags",'--build-property',"compiler.c.extra_flags=$cFlags",
    '--build-property','build.sdk=NONOSDK22x_190703','--build-property','build.stdcpp_level=-std=gnu++17')
Invoke-Build $cli ($espCommon + @('--build-path','build/fnr005-esp-win','diagnostics/transport-qualification/esp')) 'build/fnr005-esp-build.log'
$stage = Join-Path $repo 'build/fnr005_production_esp'
New-Item -ItemType Directory -Path $stage -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $stage 'fnr005_production_esp.ino'),
    "#include `"$root/firmware/esp8266/radio_adapter.cpp`"`n#include `"$root/firmware/esp8266/transport_adapter.cpp`"`n#include `"$root/firmware/esp8266/main.cpp`"`n",[Text.UTF8Encoding]::new($false))
foreach ($unit in 'datagram','radio_ingress','transport','transport_tx') {
    [IO.File]::WriteAllText((Join-Path $stage "$unit.c"),"#include `"$root/shared/src/$unit.c`"`n",[Text.UTF8Encoding]::new($false))
}
Invoke-Build $cli ($espCommon + @('--build-path','build/fnr005-production-esp-win',$stage)) 'build/fnr005-production-esp-build.log'
$artifacts = @('build/fnr005-production-rp-win/fnr_rp2040_hid.uf2','build/fnr005-production-rp-win/fnr_rp2040_hid.elf',
    'build/fnr005-rp-win/fnr_transport_observer.uf2','build/fnr005-rp-win/fnr_transport_observer.elf',
    'build/fnr005-esp-win/esp.ino.bin','build/fnr005-esp-win/esp.ino.elf',
    'build/fnr005-production-esp-win/fnr005_production_esp.ino.bin','build/fnr005-production-esp-win/fnr005_production_esp.ino.elf')
$files = @($artifacts | ForEach-Object { $file = Get-Item -LiteralPath $_; @{path=$file.FullName;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()} })
$compiler = Get-ChildItem '.deps/hardware/arduino-data/packages/esp8266/tools/xtensa-lx106-elf-gcc' -Filter 'xtensa-lx106-elf-gcc.exe' -Recurse
if (@($compiler).Count -ne 1) { throw 'Ambiguous Xtensa compiler' }
$tools = @($python,$cmake,$cli,(Join-Path $arm 'arm-none-eabi-gcc.exe'),$compiler.FullName) | ForEach-Object {
    @{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$backup = Get-Content -Raw -LiteralPath (Join-Path $PrivateRadioBackup 'backup.json') | ConvertFrom-Json
$manifest = @{source_commit=$source; source_tree=(& git rev-parse 'HEAD^{tree}').Trim(); source_clean=$true;
    lock_sha256=(& $python -c 'import hashlib,subprocess; print(hashlib.sha256(subprocess.check_output(["git","show","HEAD:dependencies.lock.json"])).hexdigest())').Trim();
    working_lock_sha256=(Get-FileHash dependencies.lock.json -Algorithm SHA256).Hash.ToLowerInvariant();
    rp_sha256=($files | Where-Object path -Like '*fnr_transport_observer.uf2').sha256;
    esp_sha256=($files | Where-Object { (Split-Path $_.path -Leaf) -eq 'esp.ino.bin' }).sha256;
    radio_mac=$backup.reads[0].mac; board=$backup.board; artifacts=$files; tools=$tools;
    commands=$commands; recorded_at_utc=[DateTime]::UtcNow.ToString('o'); physical_acceptance=$false}
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $destination -Encoding utf8NoBOM
Write-Output 'Built and hashed production/qualification targets; physical acceptance remains unestablished'
