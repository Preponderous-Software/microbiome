# Releasing the Strata build

A release is a bare version tag on `master` (`0.1.0`, never `v0.1.0`) with a GitHub release on it.
Publishing the release starts [`.github/workflows/release.yml`](../.github/workflows/release.yml),
which builds `mb_strata`, the Strata micro-environment ([STRATA.md](STRATA.md)), for Linux x64 and
attaches the download. Only Linux is built: the Strata build is built and tested on Linux only
(`.github/workflows/strata.yml`). The console app, `mb_webapp`, the Docker image and the browser
build are not part of a release download. The browser build deploys on every push to `master`.

The version a release carries is `project(microbiome_strata VERSION ...)` in
[`strata/CMakeLists.txt`](../strata/CMakeLists.txt), the version recorded in every replay's
metadata. The repository's `version.txt` (0.0.1: the version the Makefile builds into `mb_app` and the
arcade deploy reports) is separate and is not
read by the workflow.

## Steps

1. **Set the version.** On a branch, set the version in `project(microbiome_strata VERSION ...)`
   in `strata/CMakeLists.txt` and open a pull request. The release workflow refuses a tag that
   does not match it.

2. **Create the release on the merged commit.** Pass the full SHA, not a branch name, so the tag
   lands on exactly the commit that was checked:

   ```bash
   git fetch origin
   sha=$(git rev-parse origin/master)
   gh release create 0.1.0 --target "$sha" --prerelease \
     --title "Microbiome 0.1.0" --notes-file notes.md
   ```

3. **Wait for the build.** The `Release` workflow runs on the published release and attaches:

   | Asset | Contents |
   | --- | --- |
   | `Microbiome-<version>-linux-x64.tar.gz` | `mb_strata`, `README.txt`, `LICENSE.txt`, `THIRD-PARTY.md` |
   | `Microbiome-<version>-linux-x64.tar.gz.sha256` | The SHA-256 of the archive |

   If the workflow fails, fix the cause and re-run it (`gh run rerun <id>`); uploads replace assets
   of the same name.

Pushing a version tag without creating a release also works: the workflow builds the tag and
creates a pre-release for it with generated notes.

## What is in the download

Strata and SDL3 are linked into the executable, so the download is the one executable,
[`README-strata-download.txt`](README-strata-download.txt) as `README.txt`, the licence and
[`THIRD-PARTY-STRATA.md`](THIRD-PARTY-STRATA.md) as `THIRD-PARTY.md`. It needs an OpenGL 4.5
driver and glibc 2.34 or newer (the build runs on Ubuntu 22.04).

What each run checks: the packaged binary links only the C and C++ system libraries, reaches the
pinned state hash for seed 1 after 2,000 ticks (`strata/test/state-seed1-2000.txt`, the same check
`strata.yml` and the browser build make), and renders a frame headless on Mesa llvmpipe.

## Checking the packaging without a release

A pull request that changes `release.yml`, `strata/CMakeLists.txt`, `strata/strata.ref` or the
download's README and notices runs the same build as a dry run, keeping the archive as a workflow
artifact. So does running it by hand, once the workflow is on `master`:

```bash
gh workflow run release.yml --ref <branch>              # dry run
gh workflow run release.yml --ref master -f tag=0.1.0   # build a tag and attach to its release
```
