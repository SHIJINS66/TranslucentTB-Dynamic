# Installer recipe (Inno Setup6)

Build the complete solution first (../docs/BUILD.md). setup.iss is the actual recipe
used for this release. It embeds the matching source archive as Source.zip.
To reconstruct the installer workspace, use a work/<name>/ directory containing
setup.iss, ChineseSimplified.isl, prerequisites.ps1 and stage/. Set up the matching
outputs/TranslucentTB-Dynamic-2026.2.5-rc1/ directory two levels above it.
Put the matching TranslucentTB-Dynamic-2026.2.5-rc1-Source.zip there before compilation.

stage/app/ contains application runtime output, original Assets, app.ico,
GPL LICENSE.txt, NOTICE.txt and ThirdPartyNotices, settings.json from this folder,
and the app-local x64 Visual C++ runtime from Visual Studio2026 redist14.51.36231.
The6 application binaries/resources are identified in payload-hashes.json.
stage/prerequisites/ contains Microsoft.UI.Xaml.2.8.appx,
Microsoft.VCLibs.x64.14.00.appx and Microsoft.VCLibs.x64.14.00.Desktop.appx from
the restored Microsoft runtime packages, plus prerequisites.b64 containing the
UTF-16LE base64 of prerequisites.ps1. Do not include a machine's prerequisite
result JSON, personal settings, logs, backups or build caches.
ALL-PAYLOAD-SHA256.json records every packaged payload for this release.
Microsoft runtime licenses and original third-party notices must be retained.

Compile with ISCC.exe setup.iss. The recipe output directory/base filename
identify the2.5-rc1 community preview; no signing credentials are included.
The application file-version scheme itself remains upstream's1.0.0.1.
An uncached installer reconstruction on a different machine has not been tested.

To select a different output/source-archive directory, compile with
`ISCC.exe /DReleaseDir="C:\path\to\release" setup.iss`. The selected folder must
contain the matching source ZIP before compilation.
