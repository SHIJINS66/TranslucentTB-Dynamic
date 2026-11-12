[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][long]$TargetWindowHandle,
 [string]$InstallDirectory=(Join-Path $env:LOCALAPPDATA 'Programs\TranslucentTBDynamic')
)
$ErrorActionPreference='Stop'
$config=Join-Path $InstallDirectory 'settings.json'
$driverExe=Join-Path $PSScriptRoot 'TaskbarSwitchDriver.exe'
$targetWindow=$TargetWindowHandle
$battery=@(Get-CimInstance -Namespace root\wmi -ClassName BatteryStatus)
if($battery.Count -ne 1 -or $battery[0].PowerOnline -or -not $battery[0].Discharging){throw 'Waiting for battery power; no test started'}
$app=Get-Process -Name TranslucentTB
if(@($app).Count -ne 1){throw 'Expected one app'}
$appId=$app.Id;$session=$app.SessionId
$exeHash=(Get-FileHash -LiteralPath $app.Path).Hash
if($exeHash -ne '23817D7A63F2C274D3698FCC48BA2DD6CD9B02271C54E4CC351942A50E4773A3'){throw 'Unexpected runtime version'}
$original=[IO.File]::ReadAllBytes($config)
$raw=[Text.Encoding]::UTF8.GetString($original).TrimStart([char]0xfeff)
$json=($raw -split "`n" | Where-Object {$_ -notmatch '^\s*//'}) -join "`n" | ConvertFrom-Json
if($json.disable_saving -or $json.dynamic_tint.sample_interval -ne 0){throw 'Unexpected config override'}
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;using System.Text;
public static class SwitchTelemetry {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string c,string t);
 public static IntPtr Worker(){return FindWindow("TTB_WorkerWindow",null);}
 public static IntPtr Taskbar(){return FindWindow("Shell_TrayWnd",null);}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string s);
 [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr h,uint m,UIntPtr w,IntPtr l,uint f,uint ms,out UIntPtr r);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder s,int n);
 public static string ForegroundClass(){var s=new StringBuilder(256);GetClassName(GetForegroundWindow(),s,256);return s.ToString();}
 [StructLayout(LayoutKind.Sequential)] public struct Point{public int X,Y;}
 [StructLayout(LayoutKind.Sequential)] public struct Rect{public int L,T,R,B;}
 [DllImport("user32.dll")] static extern bool GetCursorPos(out Point p);
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h,out Rect r);
 public static bool OnBar(){Point p;Rect r;return GetCursorPos(out p)&&GetWindowRect(Taskbar(),out r)&&p.X>=r.L&&p.X<r.R&&p.Y>=r.T&&p.Y<r.B;}
 [DllImport("kernel32.dll")] public static extern bool GetSystemTimes(out ulong idle,out ulong kernel,out ulong user);
}
'@
$message=[SwitchTelemetry]::RegisterWindowMessage('TranslucentTB.DynamicTint.Stats.v1')
$full=(Get-CimInstance -Namespace root\wmi -ClassName BatteryFullChargedCapacity).FullChargedCapacity
$cap=(Get-CimInstance -Namespace root\wmi -Query 'SELECT Capabilities FROM BatteryStaticData').Capabilities
if(@($cap).Count -ne 1 -or ([uint64]$cap -band 0x40000000L) -ne 0){throw 'Unverified battery units'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out){throw 'Choose a fresh output directory'}
New-Item -ItemType Directory -Path $out | Out-Null
$backup=Join-Path $out 'settings-before-test.bak'
[IO.File]::WriteAllBytes($backup,$original)
$originalHash=(Get-FileHash -LiteralPath $config).Hash
if((Get-FileHash -LiteralPath $backup).Hash -ne $originalHash){throw 'Backup verification failed'}
$driver=$null;$lastBattery=$null;$lastBatteryTime=-1.0
function CPU($name){
 try{
  $ps=@(Get-Process -Name $name -ErrorAction Stop | Where-Object {$_.SessionId -eq $session})
  if($ps.Count -eq 0){return $null}
  $sum=0.0
  foreach($p in $ps){$v=$p.TotalProcessorTime;if($null -eq $v){return $null};$sum+=$v.TotalSeconds}
  return $sum
 }catch{return $null}
}
function Snapshot {
 $t=[Diagnostics.Stopwatch]::GetTimestamp()/[double][Diagnostics.Stopwatch]::Frequency
 if($null -eq $script:lastBattery -or $t-$script:lastBatteryTime -ge 5){
  $b=Get-CimInstance -Namespace root\wmi -ClassName BatteryStatus
  if($b.PowerOnline -or -not $b.Discharging){throw 'Power source changed'}
  if($b.RemainingCapacity -lt $full*0.25){throw 'Battery below25%'}
  $script:lastBattery=$b;$script:lastBatteryTime=$t
 }
 $p=Get-Process -Id $appId
 if($p.ProcessName -ne 'TranslucentTB'){throw 'Application identity changed'}
 if($driver -and $driver.HasExited){throw 'Click driver exited; measurement invalid'}
 $h=[SwitchTelemetry]::Worker();if($h -eq [IntPtr]::Zero){throw 'Diagnostics unavailable'}
 $s=@()
 foreach($i in 0..8){
  [UIntPtr]$v=[UIntPtr]::Zero
  if([SwitchTelemetry]::SendMessageTimeout($h,$message,[UIntPtr][uint32]$i,[IntPtr]::Zero,2,500,[ref]$v) -eq [IntPtr]::Zero){throw 'Stats timeout'}
  $s+=$v.ToUInt64()
 }
 [ulong]$idle=0;[ulong]$kernel=0;[ulong]$user=0
 if(-not [SwitchTelemetry]::GetSystemTimes([ref]$idle,[ref]$kernel,[ref]$user)){throw 'System CPU unavailable'}
 [pscustomobject]@{utc=[DateTime]::UtcNow.ToString('o');time=([Diagnostics.Stopwatch]::GetTimestamp()/[double][Diagnostics.Stopwatch]::Frequency);app_cpu_s=$p.TotalProcessorTime.TotalSeconds;explorer_cpu_s=(CPU 'explorer');dwm_cpu_s=(CPU 'dwm');sampler_cpu_s=(Get-Process -Id $PID).TotalProcessorTime.TotalSeconds;driver_cpu_s=if($driver){$driver.TotalProcessorTime.TotalSeconds}else{$null};ticks=$s[0];captures=$s[1];writes=$s[2];next_ms=$s[3];adaptive=$s[4];inputs=$s[5];gdi=$s[6];allocations=$s[7];stops=$s[8];system_idle=$idle;system_total=($kernel+$user);foreground_class=[SwitchTelemetry]::ForegroundClass();target_minimized=[SwitchTelemetry]::IsIconic([IntPtr]$targetWindow);pointer_on_taskbar=[SwitchTelemetry]::OnBar();battery_read_time=$script:lastBatteryTime;discharge_mw=$script:lastBattery.DischargeRate;capacity_mwh=$script:lastBattery.RemainingCapacity;power_online=$script:lastBattery.PowerOnline}
}
$initial=Snapshot
[ordered]@{started=[DateTime]::Now.ToString('o');app_pid=$appId;exe_sha256=$exeHash;target_window=$targetWindow;logical_processors=[Environment]::ProcessorCount;original_config_sha256=$originalHash;original_dynamic_tint=$json.dynamic_tint;full_capacity_mwh=$full;battery_capabilities=$cap;power_scheme=(powercfg /getactivescheme | Out-String).Trim();brightness=@(Get-CimInstance -Namespace root\wmi -ClassName WmiMonitorBrightness | Select-Object CurrentBrightness);design='off,adaptive,continuous,continuous,adaptive,off; each phase8s settle+48s measurement; click every4s; pointer remains on taskbar; sample every1s; battery poll every5s';initial=$initial} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $out 'metadata.json') -Encoding UTF8
$rows=[Collections.Generic.List[object]]::new();$runs=[Collections.Generic.List[object]]::new()
$lastHash=$originalHash;$restored=$false;$failure=$null;$run=0
Write-Output "Output: $out"
try{
 $driver=Start-Process -FilePath $driverExe -ArgumentList @('--run',$out,$targetWindow) -WindowStyle Hidden -RedirectStandardOutput (Join-Path $out 'driver-output.txt') -RedirectStandardError (Join-Path $out 'driver-stderr.txt') -PassThru
 $limit=[DateTime]::UtcNow.AddSeconds(12)
 while(-not (Test-Path -LiteralPath (Join-Path $out 'driver-ready.flag'))){
  if($driver.HasExited -or [DateTime]::UtcNow -gt $limit){throw 'Click driver failed to become ready'}
  Start-Sleep -Milliseconds 100
 }
 foreach($profile in @('off','adaptive','continuous','continuous','adaptive','off')){
  if((Get-FileHash -LiteralPath $config).Hash -ne $lastHash){throw 'External settings edit detected'}
  $run++;$mode=($profile -eq 'adaptive');$enabled=($profile -ne 'off')
  $parts=$raw -split '(?="dynamic_tint"\s*:)',2
  if($parts.Count -ne 2){throw 'Dynamic tint block missing'}
  $tail=$parts[1]
  foreach($pair in @(@('enabled',$enabled),@('adaptive_sampling',$mode))){
   $regex=[regex]::new('("'+$pair[0]+'"\s*:\s*)(true|false)')
   if($regex.Matches($tail).Count -ne 1){throw 'Unexpected field count'}
   $tail=$regex.Replace($tail,('${1}'+$pair[1].ToString().ToLowerInvariant()),1)
  }
  [IO.File]::WriteAllText($config,$parts[0]+$tail,[Text.UTF8Encoding]::new($true));$lastHash=(Get-FileHash -LiteralPath $config).Hash
  Start-Sleep -Seconds 8
  $base=Snapshot;if([bool]$base.adaptive -ne $mode){throw 'Mode reload failed'}
  $runs.Add([pscustomobject]@{run=$run;profile=$profile;baseline=$base})
  $runs | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'runs.json') -Encoding UTF8
  Write-Output "Run $run/6: $profile,48 seconds"
  $clock=[Diagnostics.Stopwatch]::StartNew()
  foreach($i in 1..48){
   $wait=1000*$i-$clock.Elapsed.TotalMilliseconds
   if($wait -gt 0){Start-Sleep -Milliseconds ([int]$wait)}
   $s=Snapshot;if([bool]$s.adaptive -ne $mode){throw 'Unexpected mode change'}
   $row=[ordered]@{run=$run;profile=$profile;sample=$i};foreach($prop in $s.PSObject.Properties){$row[$prop.Name]=$prop.Value}
   $rows.Add([pscustomobject]$row)
   if($i%4 -eq 0){$rows | Export-Csv -LiteralPath (Join-Path $out 'samples.csv') -NoTypeInformation -Encoding UTF8}
  }
 }
}catch{$failure=$_.Exception.Message;Write-Output "Stopped: $failure"}
finally{
 [IO.File]::WriteAllText((Join-Path $out 'stop-clicks.flag'),'stop')
 if($driver -and -not $driver.HasExited){if(-not $driver.WaitForExit(6000)){Write-Output 'Driver cleanup still pending'}}
 if((Get-FileHash -LiteralPath $config).Hash -eq $lastHash){[IO.File]::WriteAllBytes($config,$original);$restored=((Get-FileHash -LiteralPath $config).Hash -eq $originalHash)}
 $rows | Export-Csv -LiteralPath (Join-Path $out 'samples.csv') -NoTypeInformation -Encoding UTF8
 [ordered]@{finished=[DateTime]::Now.ToString('o');samples=$rows.Count;config_restored=$restored;failure=$failure;driver_exited=if($driver){$driver.HasExited}else{$null};driver_exit_code=if($driver -and $driver.HasExited){$driver.ExitCode}else{$null};output=$out} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'completion.json') -Encoding UTF8
 Write-Output "Finished; samples=$($rows.Count); config restored=$restored; failure=$failure"
}
