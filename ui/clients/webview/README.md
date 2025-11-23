## Dependencies
This project requires WebKitGTK development packages on Linux. Install them based on your platform:

## Windows
No additional dependencies required! The project uses the native webview2 backend and will automatically download WebView2 during build via CMake's FetchContent.

## Linux Distributions
This project requires WebKitGTK development packages. Install them based on your distribution:

### Fedora / RHEL / CentOS

```bash
# Fedora 43+
sudo dnf install webkit2gtk4.1-devel

# RHEL 9 / CentOS Stream 9
sudo dnf install webkit2gtk4.1-devel

# Older versions (RHEL 8 / CentOS 8)
sudo dnf install webkit2gtk3-devel
```

## Debian / Ubuntu

```
# Ubuntu 22.04+ / Debian 12+
sudo apt install libwebkit2gtk-4.1-dev

# Older versions (Ubuntu 20.04 / Debian 11)
sudo apt install libwebkit2gtk-4.0-dev
```

## Arch Linux

```
sudo zypper install webkit2gtk3-devel
```

## openSUSE

```
sudo zypper install webkit2gtk3-devel
```

## Verification
```
pkg-config --exists webkit2gtk-4.1 && echo "WebKitGTK found" || echo "WebKitGTK not found"
```

## Build instructions

```
# Linux
./build_rel.sh

# Windows
build_rel.bat
```

*Note*: The project uses CMake's FetchContent to download the webview library, but on Linux you still need the system WebKitGTK development packages for linking. Windows automatically handles WebView2 dependencies through the Microsoft WebView2 runtime.
