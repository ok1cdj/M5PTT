# How the install page ships

This page (`index.html` + `manifest.json`) flashes M5PTT to an AtomS3 Lite
over Web Serial using [ESP Web Tools](https://esphome.github.io/esp-web-tools/).
Everything on the page is static except the firmware image and the version
number, both produced by CI.

## The pipeline (two workflows)

Firmware builds and page deploys are decoupled, because the firmware is stable
while the page and manual change often. The firmware binary is handed off
between them as a **GitHub Release asset**.

**`.github/workflows/firmware.yml`** — runs only on a pushed tag `v*`:

1. builds the firmware with `pio run -e atoms3-lite`;
2. attaches the merged image `firmware.factory.bin` to the GitHub Release for
   that tag (creating the release if needed).

It does not touch Pages. Run it only for real firmware releases.

**`.github/workflows/pages.yml`** — deploys `docs/` to GitHub Pages. Triggers:

- a `push` to `main` touching `docs/**` (edit the page or manual, just push);
- `workflow_dispatch` (manual button);
- `workflow_run` after `firmware.yml` succeeds (so a release refreshes the page).

It downloads `firmware.factory.bin` from the **latest** release, reads the
version from that release tag (`v1.2.3` -> `1.2.3`), stamps it into
`manifest.json`, and deploys `docs/` + `firmware/`.

No binaries are committed to the repo — they live only as release assets and in
the Pages artifact. All `pages.yml` triggers run in the default-branch context,
so the `github-pages` environment's default-branch rule is enough (no `v*` tag
protection rule needed).

### First-time bootstrap

`pages.yml` needs the latest release to actually carry a `firmware.factory.bin`
asset. Tags created before this split (e.g. `v0.1.0` from the old single
workflow) have no asset, so run `firmware.yml` once — re-push the tag
(`git push -f origin v0.1.0`) or cut a new one — to attach the binary before the
page deploy can succeed.

## Why one merged file, not four parts

`firmware.factory.bin` is a full-flash image starting at offset `0` (bootloader,
partition table, `boot_app0`, and the app already combined). ESP Web Tools
flashes it as a single part:

```json
{ "path": "firmware/firmware.factory.bin", "offset": 0 }
```

This avoids the per-file offset table entirely and sidesteps `boot_app0.bin`,
which is **not** a build artifact (it ships inside the framework package, so a
multi-part manifest can't reference it from CI).

## Versioning

Cut a release by pushing a tag: `git tag v1.2.3 && git push origin v1.2.3`.
The page fetches `manifest.json` at load time and shows the version next to the
install button, so it lives in exactly one place.

## Hosting

Served from **https://m5ptt.ok1cdj.com** via GitHub Pages. Web Serial requires
HTTPS, which Pages provides. One-time setup:

- **DNS:** `m5ptt.ok1cdj.com` CNAME → `ok1cdj.github.io.`
- **Repo Settings → Pages:** Source = *GitHub Actions*; Custom domain =
  `m5ptt.ok1cdj.com`; enable *Enforce HTTPS* once the cert is issued.
- `docs/CNAME` keeps the custom domain bound on every deploy.

## Optional: serialType

`manifest.json` build entries can carry a `"serialType": "cdc"` field to skip
auto-detection when a board only exposes native USB CDC (which AtomS3 Lite
does). Not required — omitting it just means ESP Web Tools treats the build as
valid for any connection type — but worth knowing if a second chip variant with
a UART bridge ever gets added here.
