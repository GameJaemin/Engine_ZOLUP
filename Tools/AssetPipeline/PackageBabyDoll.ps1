[CmdletBinding()]
param(
    [string]$OutputDirectory,
    [string]$CrtDirectory,
    [string]$RuntimeDirectory
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$runtimeSource = if ($RuntimeDirectory) { [IO.Path]::GetFullPath($RuntimeDirectory) } else { Join-Path $repositoryRoot 'Binaries/v143/x64/Release' }
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot ('Binaries/Packages/BabyDoll_Windows_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
}
$packageRoot = [IO.Path]::GetFullPath($OutputDirectory)
if ((Test-Path -LiteralPath $packageRoot) -or (Test-Path -LiteralPath ($packageRoot + '.zip'))) {
    throw 'Choose a new output directory; existing packages are never overwritten.'
}
if (-not $CrtDirectory) {
    $redistRoot = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC'
    $redist = Get-ChildItem -LiteralPath $redistRoot -Directory |
        Where-Object { $_.Name -match '^14\.4\d\.' } |
        Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
    if (-not $redist) { throw 'Supply -CrtDirectory pointing to the x64 Microsoft.VC143.CRT redistributable folder.' }
    $CrtDirectory = Join-Path $redist.FullName 'x64/Microsoft.VC143.CRT'
}
$required = @(
    "$runtimeSource/SeonEngine.exe", "$runtimeSource/dxcompiler.dll", "$runtimeSource/dxil.dll",
    "$runtimeSource/Render/D3D12/shader/defaultWorld_VS.hlsl",
    "$CrtDirectory/msvcp140.dll", "$CrtDirectory/vcruntime140.dll", "$CrtDirectory/vcruntime140_1.dll",
    "$repositoryRoot/Game/Content/BabyDoll/Scissorman/Scissorman.gltf",
    "$repositoryRoot/Game/Content/BabyDoll/Config/Prototype.json",
    "$repositoryRoot/Game/Content/BabyDoll/Config/Escape.json",
    "$repositoryRoot/Game/Content/BabyDoll/Config/ObjectSounds.json",
    "$repositoryRoot/Game/Content/BabyDoll/VoxelDoll/Body.gltf",
    "$repositoryRoot/Game/Content/BabyDoll/VoxelDoll/StitchTorsoLeft.gltf"
)
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required runtime file missing: $path. Build Release first." }
}

# Paths::GetRootDirectory requires this exact nesting; do not flatten the executable folder.
$runtimeTarget = Join-Path $packageRoot 'Binaries/v143/x64/Release'
New-Item -ItemType Directory -Path $runtimeTarget -Force | Out-Null
foreach ($name in @('SeonEngine.exe', 'dxcompiler.dll', 'dxil.dll')) {
    Copy-Item -LiteralPath (Join-Path $runtimeSource $name) -Destination $runtimeTarget
}
Copy-Item -LiteralPath "$runtimeSource/Render" -Destination $runtimeTarget -Recurse
Get-ChildItem -LiteralPath $CrtDirectory -Filter '*.dll' -File |
    Copy-Item -Destination $runtimeTarget
foreach ($relative in @('Game/Content/BabyDoll', 'Engine/Content')) {
    $destination = Join-Path $packageRoot (Split-Path $relative -Parent)
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repositoryRoot $relative) -Destination $destination -Recurse
}
$notices = Join-Path $packageRoot 'ThirdPartyNotices'
New-Item -ItemType Directory -Path $notices | Out-Null
$share = Join-Path $repositoryRoot 'Intermediate/vcpkg_installed/v143/x64-windows-static-md/share'
if (Test-Path -LiteralPath $share) {
    Get-ChildItem -LiteralPath $share -Directory | ForEach-Object {
        $copyright = Join-Path $_.FullName 'copyright'
        if (Test-Path -LiteralPath $copyright) {
            Copy-Item -LiteralPath $copyright -Destination (Join-Path $notices ($_.Name + '.txt'))
        }
    }
}
$launcher = @'
@echo off
cd /d "%~dp0"
"%~dp0Binaries\v143\x64\Release\SeonEngine.exe" %*
'@
[IO.File]::WriteAllText((Join-Path $packageRoot 'Play.cmd'), $launcher.Replace("`n", "`r`n") + "`r`n", [Text.Encoding]::ASCII)
$instructions = @'
BabyDoll prototype - Windows x64 / DirectX 12

Extract the entire ZIP, then double-click Play.cmd.
Keep the folder structure intact. SeonEngine.exe needs the included assets,
shader files, DXC and Visual C++ runtime DLLs. Visual Studio is not required.
This package starts the BabyDoll prototype; editor and Bistro assets are omitted.

Select Single player (S / D) or Co-op (S / period), then Begin escape.
Single player: alternate S and D; left/right mouse buttons control left/right hand.
Co-op: alternate S and period; R is left hand, T is right hand.
Use a free hand to pick up a key, and that hand to use it at the gate.
Either hand grabs ladders / enters crawl passages. Left hand grabs/releases bars.
Reach the hidden elevator upstairs. The layout is a feature test layout.
Mouse: look. Esc: pause / resume. Tab: diagnostics.
Esc > Developer tools > Feature test positions jumps to a feature for testing.
The Ignore capture option is for testing only; normal play can catch the player.
Checkpoints: Binaries/BabyDollSave/checkpoint.json. Reset test room starts fresh.
Settings: Game/Content/BabyDoll/Config/*.json (restart after editing).

SCISSORMAN attribution and source information:
Game/Content/BabyDoll/Scissorman/Source.json
Library notices: ThirdPartyNotices/
'@
[IO.File]::WriteAllText((Join-Path $packageRoot 'README.txt'), $instructions, [Text.UTF8Encoding]::new($false))
Compress-Archive -LiteralPath $packageRoot -DestinationPath ($packageRoot + '.zip') -CompressionLevel Optimal
Write-Output "Package: $packageRoot"
Write-Output "Archive: $packageRoot.zip"
