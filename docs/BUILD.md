# Build and package

The source snapshot targets TranslucentTB 2026.2 commit `d4636e439865df0a1a1419db408e055740ce5c74`, not current upstream develop. `src/` contains 422 application/source-project files without restored caches or generated binaries. Some upstream dependency-configuration symlinks are materialized as equivalent files for Windows ZIP extraction. `SOURCE-SHA256.json` identifies their exact contents.

## Application

Use Visual Studio 2026 and import `src/.vsconfig`. Follow `src/CONTRIBUTING.md`, restore NuGet packages and bootstrap/integrate vcpkg. Pinned dependencies remain in the original manifests and `packages.config`. The validated build reused restored local caches; a clean, uncached build on another computer has not been tested.

From a developer shell in `src/`, build the **complete solution serially**:

```text
MSBuild.exe TranslucentTB.slnx -p:Configuration=Release -p:Platform=x64 -p:AppxPackageSigningEnabled=false -m:1
x64\Release\Tests.exe --gtest_brief=1
```

Individual-project builds previously produced inconsistent shared intermediates. Runtime files are under `AppPackage/bin/x64/Release`. Unpackaged execution requires compatible WinUI 2.8, VCLibs frameworks and the native VC runtime. Do not run two TranslucentTB instances together. If a different ExplorerTAP DLL is already loaded, restart Explorer or sign out/in before evaluating the replacement.

## Source and installer

This repository is a review/distribution layout with the application inside `src/`; the patches in `patches/` apply at an upstream checkout's root. Choose exactly one matching baseline, then run `git apply --check` and `git apply`. See [UPSTREAM.md](UPSTREAM.md).

`installer/setup.iss` is the recipe for this candidate. It embeds the matching source archive as `Source.zip`, includes original licences/notices, and preserves existing settings. `installer/README.md` describes staging files and runtime dependencies. The application binaries are unchanged from the user-tested 2026.2.5 hotfix; rc1 changes documentation and packaging only.

Native application file-version numbers retain upstream's scheme. The community installer reports `2026.2.5-rc1`; this is not an upstream version or a claim of code signing.

## Attribution and modifications

Keep upstream copyright and licence files, third-party notices and Microsoft runtime licences with redistribution. [MODIFICATIONS.md](../MODIFICATIONS.md) records the modification date, scope and baseline. The available dependency source archives remain under `vendor-source-archives/` and do not replace NuGet/vcpkg restore.

## Verify an archive or Git checkout

`python tools/verify_snapshot.py` checks application contents from a Git checkout,
normalizing CRLF/LF for UTF-8 files according to SOURCE-CONTENT-SHA256.json. This
avoids treating Git/editor line-ending conversion as an application logic change.
The original byte-exact SOURCE-SHA256.json and REPOSITORY-SHA256.json describe
the downloadable source ZIP; use `python tools/verify_snapshot.py path/to/Source.zip`
to verify that artifact's CRC and all inventoried bytes without normalization.
The repository manifest excludes itself to avoid a self-referential hash.
