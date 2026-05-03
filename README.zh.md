# GXDE 文件传输

GXDE 文件传输是一个基于 DTK/Qt 的桌面文件传输工具，用于并排浏览本地文件和远程文件。它支持 FTP、SFTP、WebDAV 和 WebDAVS 连接，并提供保存连接、拖拽传输和传输队列等功能。

英文文档请见 [README.md](README.md)。

## 功能

- 本地与远程双栏文件浏览。
- 支持 FTP、SFTP、WebDAV 和 WebDAVS 的快速连接栏。
- 通过 DTK 设置弹窗管理保存的连接。
- 支持上传、下载和递归文件夹传输。
- 支持在本地和远程面板之间拖拽文件。
- 支持通过 XDS 直接保存将远程文件拖拽到 GXDE 文件管理器。
- 支持远程删除、移动和文件夹大小计算。
- 提供传输列表和连接日志，便于跟踪操作状态。

## 构建

```bash
qmake remote-file-dtk2.pro
make -j$(nproc)
```

## 运行

```bash
./gxde-filetransfer
```

## 打包

```bash
dpkg-buildpackage -us -uc
```

生成的 Debian 软件包会输出到项目上级目录。

## 依赖

- Qt 5 Widgets、Network 和 XML 模块。
- DTK Widget/Core 开发库。
- 用于远程协议操作的 `curl`。

## 许可证

许可证信息请见 Debian copyright 文件。
