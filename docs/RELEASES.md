# Releases and test builds

`main` holds development work; `stable` holds reviewed snapshots, promoted
through a `main` → `stable` pull request. Releases are made from `stable`.

## What CI builds

The **Build binaries** workflow builds all five packages (Linux x86_64, Steam
Deck, Windows x64, macOS Apple Silicon and Intel) for every pull request into
`stable` and every push to `stable`. They appear under
**Actions → Build binaries → run → Artifacts**: a pull request's for 7 days,
`stable`'s for 14 days. A separate job compiles the Windows host-service tests
natively on Windows.

- **Windows** packages are cross-compiled on Ubuntu with the pinned
  LLVM-MinGW toolchain. They include its C++ runtime DLLs (`libc++`,
  `libunwind`, `winpthread`) and the pinned DirectX shader compilers
  (`dxcompiler.dll`, `dxil.dll`, `d3dcompiler_47.dll`) with their notices;
  packaging verifies their versions and rejects missing or modified DLLs.
- **macOS Intel** is cross-compiled on the Apple Silicon runner; Rosetta runs
  its executable check.
- **Steam Deck** is a Linux x86_64 build made on Ubuntu 24.04 with GCC 14.
  `package_build.py --steamdeck` reads each binary's dependencies and rejects
  symbol versions newer than SteamOS 3.7 (glibc 2.40, GCC 14 runtime) or other
  system libraries.

CI fetches each dependency at its pinned commit only
(`setup_dependencies.py --shallow`). Every package contains
`SOURCE-MANIFEST.json` (version, source commit, pinned dependencies, patches
and the hash of every file), the third-party notices in `LICENSES/`, and no
game data. Packages are not signed.

## Test builds

A pull request from this repository also publishes its packages as the
**pre-release** `test-pr<N>-<commit>`. Each push replaces it, and the
*Remove test builds* workflow deletes it when the pull request closes.
Pre-releases never count as the latest release. Anyone can download them, and
the launcher's **Test** channel installs them in place
([updates](SETTINGS.md#updates)).

Every build records its origin for the launcher's About page and update check:
`release` (a push to `stable`), `test` (a pull request build, with its number)
or `dev` (any other build), plus the commit, run and build time
(`MSCHARGED_BUILD_*` in `CMakeLists.txt`). Pull request builds compile the
pull request's head commit, so their version names a commit of `main`.

## Versions and releases

The version is the `project(mscharged_port VERSION x.y.z ...)` line in
`CMakeLists.txt`, following [Semantic Versioning](https://semver.org): a
**patch** for bug fixes (1.0.0 → 1.0.1), a **minor** for new features
(1.0.1 → 1.1.0), a **major** for breaking changes. To release:

1. On `main`, raise the version and commit it with the changes it covers:

   ```sh
   python3 tools/bump_version.py patch   # or minor, major, x.y.z
   ```

2. Open the `main` → `stable` pull request; its run builds the packages and
   publishes them as a test build. Merge it.
3. The merge's `stable` run builds the packages once more and, as that version
   has no release yet, drafts the GitHub release `v<version>` on the merge
   commit, with the packages, `SHA256SUMS` and notes generated from the changes
   since the last release. No tag needs to be pushed by hand.
4. Under **Releases**, edit the draft (do not create a new release), add a few
   words and publish it; publishing creates the tag. A merge that keeps an
   already released version builds packages but drafts nothing.

The README's release and download badges follow the latest published release
by themselves.
