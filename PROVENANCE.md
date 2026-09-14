# Provenance

- **Upstream:** Ubiquiti's GPL source release `UCKP-2.5.11-GPL.tar.gz`
  (UniFi Cloud Key Plus, UCKP).
- **Member used:** `UCKP-2.5.11-GPL/linux-qcom-apq8053-3.18.44-ui-qcom.tar`,
  extracted here with its top-level directory stripped (tree at repo root).
- **Bundle sha256:** `fdd9f0b7874d78e653f5e44a53e88f901607b89dcdabfe03371826b1ae96e0d5`
- **Kernel tar sha256:** `d1d733355c29919cc3d2ce461bcf2de8e5e4485b3c2dc6ceef59d5cef58e3663`
  (size 1,910,497,280 bytes)

## Modifications from the vendor tarball

Source files are **unmodified**. Only build output/generated files were
removed, so the tree is source-only and browsable:

- `make ARCH=arm64 mrproper` (removes `vmlinux`, `vmlinux.o`, `.tmp_vmlinux*`,
  `*.o`, `*.ko`, `*.a`, `*.cmd`, `built-in.o`, `System.map`, `Module.symvers`,
  `include/config`, `include/generated`, `.config`, `.tmp_versions`, …).
- Removed manually: `debian/tmp`, `debian/dbgtmp`, `debian/hdrtmp`,
  `arch/*/boot/Image*`, all `*.dtb`.
- Added: `README.md`, `PROVENANCE.md` (this file).

The broken absolute symlinks Ubiquiti left in the tree (pointing at
`/home/inaro/src/github.com/ubiquiti/...` on their build host) are preserved
as-is.

## License

GPL-2.0 — see [COPYING](COPYING). This tree is the Linux kernel and
Qualcomm/Ubiquiti vendor additions; copyright belongs to their respective
authors as stated in each file.
