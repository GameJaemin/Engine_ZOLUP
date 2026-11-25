[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Recipe,
    [string]$Blender,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $Blender) {
    $command = Get-Command blender -ErrorAction SilentlyContinue
    if ($command) { $Blender = $command.Source }
}
if (-not $Blender) {
    $installationRoot = Join-Path $env:ProgramFiles 'Blender Foundation'
    $candidate = Get-ChildItem -LiteralPath $installationRoot -Filter blender.exe -Recurse -File -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($candidate) { $Blender = $candidate.FullName }
}
if (-not $Blender -or -not (Test-Path -LiteralPath $Blender -PathType Leaf)) {
    throw 'Blender 5.1 executable not found. Pass -Blender with the full executable path.'
}
$recipePath = (Resolve-Path -LiteralPath $Recipe).Path
$scriptPath = Join-Path $PSScriptRoot 'Scripts/convert_asset.py'
$arguments = @('--background', '--factory-startup', '--python-exit-code', '1', '--python', $scriptPath, '--', '--recipe', $recipePath, '--root', $repositoryRoot)
if ($Force) { $arguments += '--force' }
& $Blender @arguments
if ($LASTEXITCODE -ne 0) { throw "Asset conversion failed (exit $LASTEXITCODE). See Intermediate/AssetPipeline for the report." }
