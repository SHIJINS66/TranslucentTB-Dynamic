$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = $env:TTBDynamicPrereqRoot
if (-not $root) { throw 'Missing prerequisite payload directory.' }
$report = Join-Path $root 'prerequisites-result.json'
$results = @()
try {
    foreach ($item in @(
        @{ File = 'Microsoft.VCLibs.x64.14.00.appx'; Name = 'Microsoft.VCLibs.140.00' },
        @{ File = 'Microsoft.VCLibs.x64.14.00.Desktop.appx'; Name = 'Microsoft.VCLibs.140.00.UWPDesktop' },
        @{ File = 'Microsoft.UI.Xaml.2.8.appx'; Name = 'Microsoft.UI.Xaml.2.8' }
    )) {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $path = Join-Path $root $item.File
        $zip = [IO.Compression.ZipFile]::OpenRead($path)
        try {
            $reader = [IO.StreamReader]::new($zip.GetEntry('AppxManifest.xml').Open())
            try { [xml]$manifest = $reader.ReadToEnd() } finally { $reader.Dispose() }
        } finally { $zip.Dispose() }
        $minimum = [version]$manifest.Package.Identity.Version
        if ($manifest.Package.Identity.Name -ne $item.Name -or $manifest.Package.Identity.ProcessorArchitecture -ne 'x64') {
            throw "Unexpected package identity: $($item.File)"
        }
        $available = @(Get-AppxPackage -Name $item.Name | Where-Object {
            $_.Architecture -eq 'X64' -and [version]$_.Version -ge $minimum
        })
        if ($available.Count -gt 0) {
            $results += @{ Name = $item.Name; Status = 'already-installed'; MinimumVersion = "$minimum" }
            continue
        }
        if ($env:TTBDynamicPrereqCheckOnly -eq '1') { throw "Missing dependency: $($item.Name) >= $minimum (x64)" }
        Add-AppxPackage -Path $path -ErrorAction Stop
        $available = @(Get-AppxPackage -Name $item.Name | Where-Object {
            $_.Architecture -eq 'X64' -and [version]$_.Version -ge $minimum
        })
        if ($available.Count -eq 0) { throw "Package did not become available: $($item.Name)" }
        $results += @{ Name = $item.Name; Status = 'installed'; MinimumVersion = "$minimum" }
    }
    @{ Success = $true; Packages = $results } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $report -Encoding UTF8
    exit 0
} catch {
    @{ Success = $false; Error = $_.Exception.Message; Packages = $results } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $report -Encoding UTF8
    exit 1
}
