[CmdletBinding()]
param([switch]$Run,[long]$TargetWindowHandle=0,[string]$OutputDirectory,[string]$InstallDirectory=(Join-Path $env:LOCALAPPDATA 'Programs\TranslucentTBDynamic'))
$ErrorActionPreference='Stop'
$framework=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319'
$exe=Join-Path $PSScriptRoot 'TaskbarSwitchDriver.exe'
if(Test-Path -LiteralPath $exe){
 $backup=$exe+'.'+(Get-Date -Format 'yyyyMMdd-HHmmssfff')+'.bak'
 Copy-Item -LiteralPath $exe -Destination $backup
 if((Get-FileHash -LiteralPath $exe).Hash -ne (Get-FileHash -LiteralPath $backup).Hash){throw 'Compiler-output backup failed'}
}
& (Join-Path $framework 'csc.exe') /nologo ('/reference:'+(Join-Path $framework 'WPF\UIAutomationClient.dll')) ('/reference:'+(Join-Path $framework 'WPF\UIAutomationTypes.dll')) ('/reference:'+(Join-Path $framework 'WPF\WindowsBase.dll')) ('/out:'+$exe) (Join-Path $PSScriptRoot 'TaskbarSwitchDriver.cs')
if($LASTEXITCODE -ne 0){throw 'Compile failed'}
& $exe --probe
if($LASTEXITCODE -ne 0){throw 'Taskbar target is not unique'}
if(-not $Run){Write-Output 'Probe only: no clicks or configuration changes. Use -Run with an explicit target window handle to measure.';return}
if($TargetWindowHandle -le 0){throw 'Pass the current target window handle explicitly'}
if(-not $OutputDirectory){$OutputDirectory=Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) ('benchmark-results\'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
& (Join-Path $PSScriptRoot 'Collect-TaskbarSwitch.ps1') -OutputDirectory $OutputDirectory -TargetWindowHandle $TargetWindowHandle -InstallDirectory $InstallDirectory
