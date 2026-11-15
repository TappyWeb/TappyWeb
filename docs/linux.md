# Linux

Download the archive or Debian package for your CPU from the [latest release](https://github.com/TappyWeb/TappyWeb/releases/latest).

| CPU | Minimum build platform | Downloads |
| --- | --- | --- |
| x64 / AMD64 | Ubuntu 22.04, glibc 2.35+ | `linux-x64.deb`, `linux-x64.tar.gz` |
| ARM64 / AArch64 | Ubuntu 24.04, glibc 2.39+ | `linux-arm64.deb`, `linux-arm64.tar.gz` |

The binary also needs the matching libstdc++ and libgcc libraries. Debian packages declare their exact requirements. On other distributions, install those system libraries or build from source.

## Debian / Ubuntu

```sh
sudo apt install ./Tappyweb-0.4.1-linux-x64.deb
tappy home.tappy
```

Use the ARM64 file on ARM machines. The package installs the terminal command, examples, documentation and a desktop/MIME entry for `.tappy` files. File managers can launch them in a terminal; existing default associations may take precedence. Remove it with `sudo apt remove tappyweb`.

## Portable

```sh
mkdir tappyweb
tar -xzf Tappyweb-0.4.1-linux-x64.tar.gz -C tappyweb
cd tappyweb
./tappy /path/to/home.tappy
```

For a per-user installation, run `./install.sh` and add `$HOME/.local/bin` to your shell's PATH. Set `TAPPY_PREFIX` to choose a different install prefix. The portable installer does not change your file associations.

## Updates and Python

Run `tappy update` to check GitHub immediately. Normal starts check in the background at most daily. Linux uses `curl`, system CA certificates, `tar` and `sha256sum`; Debian installs these dependencies for you. Updates are checked against GitHub's SHA256 digest, staged per user, and used on the next command. Servers already running are left alone. Set `TAPPY_NO_UPDATE=1` to disable checks and cached-runtime forwarding.

Python 3.10+ is needed only for apps that use Python events. The runtime and packaging tools are C++. See [Python](python.md) and [building from source](../README.md#build-from-source).
