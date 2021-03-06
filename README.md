# android-efi [![Build Status](https://travis-ci.com/me176c-dev/android-efi.svg?branch=master)](https://travis-ci.com/me176c-dev/android-efi)

android-efi is a simple x86 EFI bootloader for Android™ boot images.
It accepts the partition GUID and/or path of an Android boot image on the
command line, loads the kernel, ramdisk and command line and finally hands over
control to the kernel.

## Building
### Requirements
android-efi is built with [Meson] and [gnu-efi]:

```
meson . build
ninja -C build
```

## Usage
Run the `android.efi` binary from a boot option, the UEFI Shell or your favorite UEFI bootloader.
Pass the partition UUID and/or the path to the boot image as command line options.

### Options
- Boot from a boot image partition:

  ```
  80868086-8086-8086-8086-000000000100
  ```

- Boot from a boot image file on the EFI system partition:

  ```
  /boot.img
  ```

- Boot from a boot image file on another partition (must be FAT32, or otherwise
  supported by an UEFI driver).
  
  ```
  80868086-8086-8086-8086-000000000007/boot.img
  ```

- Add additional kernel parameters:

  ```
  /boot.img -- androidboot.mode=charger
  ```

- Load additional init ramdisks (initrd) from files. The files must be on the same partition as the `android.efi` binary.

  ```
  80868086-8086-8086-8086-000000000100 -- initrd=/intel-ucode.img initrd=/acpi.img
  ```

### systemd-boot
Example configuration for [systemd-boot]:

```
title    Android
efi      /android.efi
options  normal 80868086-8086-8086-8086-000000000100
```

In this case, the first argument is ignored because this would be `android.efi`
when using the application in an UEFI shell.

**Optional:** With the patch
["Add Android loader type that automatically configures android.efi"](https://github.com/me176c-dev/systemd-boot-me176c/commit/7ea60c70324d059542987d16d518c2677e958772)
for systemd-boot you can use similar syntactic sugar as for regular Linux configurations:

```
title    Android
android  80868086-8086-8086-8086-000000000100
initrd   /intel-ucode.img
options  androidboot.mode=charger
```

Is then equivalent to:

```
title    Android
efi      /android.efi
options  android 80868086-8086-8086-8086-000000000100 -- initrd=/intel-ucode.img androidboot.mode=charger
```

## License
```
android-efi

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
```

**Note:** The Android robot contained in `splash.png` is reproduced or modified from work created and
shared by Google and used according to terms described in the 
[Creative Commons 3.0 Attribution License](https://creativecommons.org/licenses/by/3.0/).

[Meson]: http://mesonbuild.com
[gnu-efi]: https://sourceforge.net/projects/gnu-efi/
[systemd-boot]: https://www.freedesktop.org/wiki/Software/systemd/systemd-boot/
