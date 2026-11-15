Linux now has ready-to-run downloads, built and tested on Linux.

- Added portable `.tar.gz` archives and Debian/Ubuntu `.deb` packages for x64 and ARM64.
- The Debian package installs `tappy` in the terminal and registers `.tappy` files to run in a terminal window. Your existing default-app choices can take precedence.
- The portable archive includes a per-user `install.sh` for `~/.local`. It also runs directly without installation.
- Linux updates use the correct architecture, HTTPS, GitHub SHA256 checksums and versioned staging. Running servers keep working during an update.
- The runtime, Python integration and packager remain C++. No Python build scripts were added.

**Requirements:** x64: Ubuntu 22.04 or newer (glibc 2.35+); ARM64: Ubuntu 24.04 or newer (glibc 2.39+). The Debian package records the compiler's actual library requirements. Other distributions can use the portable build if those libraries are available, or build from source. Automatic updates need `curl`, CA certificates, `tar` and `sha256sum`.

The Python features from 0.4.0 are included. Install Python 3.10+ separately if your app uses `python:` blocks or Python scripts. Ordinary apps do not need Python.

Mac and Windows downloads are also updated to 0.4.1. The Mac package is universal, unsigned and not notarized. Windows Studio is maintained separately; this release is for the language.

Every platform runs the CLI/HTTP integration tests, theme tests and updater checks before publishing. Linux also tests portable installation, Debian installation and removal, and MIME registration.

This description has been written with AI.
