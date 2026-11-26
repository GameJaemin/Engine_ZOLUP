[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$NoDebugSymbols
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$vsRoot = 'C:\Program Files\Microsoft Visual Studio\2022\Community'
$vsTools = Join-Path $vsRoot 'VC\Tools\MSVC'
$compiler = Get-ChildItem -LiteralPath $vsTools -Directory | Where-Object { $_.Name -like '14.4*' } | Sort-Object Name -Descending | Select-Object -First 1
if (-not $compiler) { throw 'Visual Studio 2022 v143 14.44 or newer 14.4x tools are required. Build with your configured IDE otherwise.' }
$buildRoot = Join-Path $repositoryRoot 'Intermediate\BabyDoll'
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
$commands = @(
    '@echo off',
    ('call "{0}\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 -vcvars_ver={1} >nul' -f $vsRoot,$compiler.Name.Substring(0,5)),
    'if errorlevel 1 exit /b 1'
)
if ($NoDebugSymbols) { $commands += 'set _LINK_=/DEBUG:NONE /INCREMENTAL:NO' }
$commands += ('"{0}\MSBuild\Current\Bin\MSBuild.exe" "{1}\SeonEngine.vcxproj" /nologo /m /p:Configuration={2} /p:Platform=x64 /p:VCToolsVersion={3} /p:MultiProcessorCompilation=true /verbosity:minimal' -f $vsRoot,$repositoryRoot,$Configuration,$compiler.Name)
$commands += 'exit /b %errorlevel%'
$buildScript = Join-Path $buildRoot 'build-local.cmd'
[IO.File]::WriteAllLines($buildScript,$commands,[Text.UTF8Encoding]::new($false))
Push-Location -LiteralPath $repositoryRoot
try {
    & $env:ComSpec /d /c $buildScript
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
}
finally { Pop-Location }
