[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$Assets,
    [string]$VsDevCmd
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (-not $VsDevCmd) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $installation = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $installation) { $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
        if ($installation) { $VsDevCmd = Join-Path $installation 'Common7/Tools/VsDevCmd.bat' }
    }
}
if (-not $VsDevCmd -or -not (Test-Path -LiteralPath $VsDevCmd)) { throw 'Visual Studio C++ tools not found. Pass -VsDevCmd.' }
$buildDirectory = Join-Path $repositoryRoot 'Intermediate/AssetPipeline/EngineProbe'
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
$engineSource = Join-Path $repositoryRoot 'Engine/Source'
$sources = @('GLTF/GLTFLoader.cpp', 'Asset.cpp', 'Animation/AnimationClip.cpp', 'Mesh/StaticMesh.cpp', 'Core/FileHelper.cpp', 'Core/Log.cpp', 'Core/Math/Matrix4x4.cpp', 'Core/Math/Transform.cpp', 'Core/Math/Quaternion.cpp', 'Core/Math/Vector3.cpp')
$arguments = @('/nologo', '/std:c++20', '/EHsc', '/utf-8', '/O2', '/DNOMINMAX', "/I`"$engineSource`"", "/I`"$(Join-Path $repositoryRoot 'Engine/ThirdParty/RapidJSON/include')`"", "/Fe:`"$(Join-Path $buildDirectory 'LoadAssetProbe.exe')`"")
$arguments += $sources | ForEach-Object { '"' + (Join-Path $engineSource $_) + '"' }
$arguments += '"' + (Join-Path $PSScriptRoot 'LoadAssetProbe.cpp') + '"'
$responseFile = Join-Path $buildDirectory 'compile.rsp'
[IO.File]::WriteAllLines($responseFile, $arguments, [Text.UTF8Encoding]::new($false))
$buildScript = Join-Path $buildDirectory 'build.cmd'
$commands = @('@echo off', "call `"$VsDevCmd`" -arch=x64 -host_arch=x64 >nul", 'if errorlevel 1 exit /b 1', "cl @`"$responseFile`"", 'exit /b %errorlevel%')
[IO.File]::WriteAllLines($buildScript, $commands, [Text.UTF8Encoding]::new($false))
Push-Location -LiteralPath $buildDirectory
try {
    & $env:ComSpec /d /c $buildScript
    if ($LASTEXITCODE -ne 0) { throw 'Engine import probe compilation failed' }
}
finally { Pop-Location }
$assetPaths = @($Assets | ForEach-Object { (Resolve-Path -LiteralPath $_).Path })
& (Join-Path $buildDirectory 'LoadAssetProbe.exe') @assetPaths
if ($LASTEXITCODE -ne 0) { throw 'Actual engine loader rejected an asset' }
