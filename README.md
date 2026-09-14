# Kernel sources for the Ubiquiti Unifi CloudKey Gen2 Plus

`linux-qcom-apq8053-3.18.44-ui-qcom` — Ubiquiti's **GPL-2.0** kernel source
for the UniFi Cloud Key Gen 2 Plus (Qualcomm APQ8053 / `msm-3.18.44` vendor
tree), extracted from Ubiquiti's `UCKP-2.5.11-GPL.tar.gz` source offer and
committed here as a browsable source tree.

This is Ubiquiti's / Qualcomm's / mainline Linux source, unmodified except
for removed build artifacts (see [PROVENANCE.md](PROVENANCE.md)). See
[COPYING](COPYING) for the license; individual files carry their own
copyright headers.

## Build artifacts

Ubiquiti shipped this tree with their own build output still in it
(`vmlinux`, `vmlinux.o`, `*.o`, `*.ko`, `debian/tmp`, …). Those were removed
with `make mrproper` plus a few manual deletions so the repository is a pure,
browsable source tree with no files over GitHub's 100 MB limit.
