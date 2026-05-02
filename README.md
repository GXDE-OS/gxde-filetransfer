# Remote File DTK2

A DTK2/Qt remote file client prototype inspired by FileZilla. It provides a desktop UI for browsing remote directories over FTP, SFTP, WebDAV and WebDAVS.

## Build

```bash
qmake remote-file-dtk2.pro
make -j$(nproc)
```

## Run

```bash
./remote-file-dtk2
```

## Current Scope

- DTK2 main window with a modern two-pane transfer layout.
- Local file browser on the left and remote browser on the right.
- Quick connection bar with saved site selection.
- Remote directory listing for `ftp`, `sftp`, `webdav`, and `webdavs` via the system `curl` command.
- Double-click folders to navigate, `Up` and `Refresh` controls, and a connection log panel.
- Upload and download selected files with a transfer list.
- Save connection credentials in the user's Qt settings for quick reuse.

Recursive folder transfers, bookmarks, host key management, and conflict handling are intentionally left for the next iteration.
