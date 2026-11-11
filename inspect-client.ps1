param([string]$ClientPath = 'X:\Games\Homesickness')
$ErrorActionPreference = 'Stop'
function Inspect-PE([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    function U16([int]$p) { [BitConverter]::ToUInt16($bytes, $p) }
    function U32([int]$p) { [BitConverter]::ToUInt32($bytes, $p) }
    function CString([int]$p) {
        $end = $p
        while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) { $end++ }
        [Text.Encoding]::ASCII.GetString($bytes, $p, $end - $p)
    }
    $pe = U32 0x3c
    $machine = U16 ($pe + 4)
    $count = U16 ($pe + 6)
    $optional = $pe + 24
    $is64 = (U16 $optional) -eq 0x20b
    $sectionTable = $optional + (U16 ($pe + 20))
    $sections = @(for ($i = 0; $i -lt $count; $i++) {
        $p = $sectionTable + 40 * $i
        [pscustomobject]@{
            Name = [Text.Encoding]::ASCII.GetString($bytes, $p, 8).Trim([char]0)
            VirtualSize = U32 ($p + 8); RVA = U32 ($p + 12)
            RawSize = U32 ($p + 16); RawOffset = U32 ($p + 20)
        }
    })
    function RvaOffset([uint32]$rva) {
        foreach ($s in $sections) {
            if ($rva -ge $s.RVA -and $rva -lt ($s.RVA + [Math]::Max($s.VirtualSize, $s.RawSize))) {
                return [int]($rva - $s.RVA + $s.RawOffset)
            }
        }
        return [int]$rva
    }
    $directory = $optional + $(if ($is64) { 112 } else { 96 })
    $imports = @()
    $importRva = U32 ($directory + 8)
    if ($importRva) {
        $descriptor = RvaOffset $importRva
        while ((U32 ($descriptor + 12)) -ne 0) {
            $dll = CString (RvaOffset (U32 ($descriptor + 12)))
            $thunkRva = U32 $descriptor
            if (-not $thunkRva) { $thunkRva = U32 ($descriptor + 16) }
            $thunk = RvaOffset $thunkRva
            $functions = @()
            while ($true) {
                $value = if ($is64) { [BitConverter]::ToUInt64($bytes, $thunk) } else { U32 $thunk }
                if (-not $value) { break }
                $ordinal = if ($is64) { ($value -shr 63) -ne 0 } else { ($value -band 0x80000000L) -ne 0 }
                if (-not $ordinal) { $functions += CString ((RvaOffset ([uint32]$value)) + 2) }
                $thunk += $(if ($is64) { 8 } else { 4 })
            }
            $imports += [pscustomobject]@{ DLL = $dll; Functions = $functions }
            $descriptor += 20
        }
    }
    $strings = @([regex]::Matches([Text.Encoding]::ASCII.GetString($bytes), '[\x20-\x7e]{5,}') | ForEach-Object { $_.Value })
    $strings += @([regex]::Matches([Text.Encoding]::Unicode.GetString($bytes), '[\x20-\x7e]{5,}') | ForEach-Object { $_.Value })
    $securityPattern = '(?i)anticheat|anti.cheat|gameguard|nprotect|xigncode|xtrap|x-trap|easyanticheat|battleye|hackshield|cheatengine|cheat engine|reshade|enbseries|CreateToolhelp32Snapshot|EnumProcessModules|WinVerifyTrust|IsDebuggerPresent|CheckRemoteDebuggerPresent|ReadProcessMemory|WriteProcessMemory|OpenProcess|NtQueryInformationProcess|GetModuleFileName|Direct3DCreate|d3d9\.dll|dxgi\.dll'
    [pscustomobject]@{
        File = [IO.Path]::GetFileName($Path)
        SHA256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
        Architecture = $(if ($machine -eq 0x14c) { 'x86' } elseif ($machine -eq 0x8664) { 'x64' } else { $machine })
        Sections = @($sections.Name)
        ImportedDLLs = @($imports.DLL)
        GraphicsImports = @($imports | Where-Object { $_.DLL -match 'd3d|dxgi' })
        SecurityRelatedImports = @($imports | ForEach-Object { $dll = $_.DLL; $_.Functions | Where-Object { $_ -match $securityPattern } | ForEach-Object { "$dll!$_" } })
        MatchingStrings = @($strings | Where-Object { $_ -match $securityPattern -and $_.Length -lt 240 } | Sort-Object -Unique | Select-Object -First 100)
    }
}
@('Neuz.exe', 'Homesickness.exe', 'NeuzD.dll') | ForEach-Object {
    Inspect-PE (Join-Path $ClientPath $_)
} | ConvertTo-Json -Depth 7
