# GXDE FileTransfer

GXDE FileTransfer is a DTK/Qt desktop file transfer tool for browsing local and remote files side by side. It supports FTP, SFTP, WebDAV and WebDAVS connections, with saved connection profiles, drag-and-drop transfers and a transfer queue.

Chinese documentation is available in [README.zh.md](README.zh.md).

## Features

- Two-pane local and remote file browser.
- Quick connection bar for FTP, SFTP, WebDAV and WebDAVS.
- Saved connection management from the DTK settings dialog.
- Upload, download and recursive folder transfer support.
- Drag files between local and remote panes.
- Drag remote files to GXDE File Manager through XDS direct save.
- Remote file operations including delete, move and folder size calculation.
- Transfer list and connection log for tracking operations.

## Build

```bash
qmake6 remote-file-dtk2.pro
make -j$(nproc)
```

## Run

```bash
./gxde-filetransfer
```

## Package

```bash
dpkg-buildpackage -us -uc
```

The generated Debian packages are written to the parent directory.

## Dependencies

- Qt 6 Widgets, Network and XML modules.
- DTK 2 Widget / DTK 6 Core development libraries (Qt 6 ports).
- `curl` for remote protocol operations.

## License

See the Debian copyright file for license details.
