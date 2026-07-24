# How the install page ships

This page (`index.html` + `manifest.json`) flashes M5PTT to an AtomS3 Lite
over Web Serial using [ESP Web Tools](https://esphome.github.io/esp-web-tools/).
Everything on the page is static except the firmware image and the version
number, both produced by CI.

## The pipeline

`.github/workflows/deploy.yml` runs on every pushed tag matching `v*` (and can
be run manually via *workflow_dispatch* for a test build). It:

1. builds the firmware with `pio run -e atoms3-lite`;
2. copies the single merged image
   `.pio/build/atoms3-lite/firmware.factory.bin` into `firmware/`;
3. stamps `manifest.json`'s `version` from the tag (`v1.2.3` -> `1.2.3`;
   manual runs get `dev`);
4. deploys `docs/` to GitHub Pages.

No binaries are committed to the repo — they only exist in the Pages artifact.

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
