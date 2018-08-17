# android-efi

android-efi is a simple x86 EFI bootloader for Android™ boot images.
It accepts the partition GUID and/or path of an Android boot image on the
command line, loads the kernel, ramdisk and command line and finally hands over
control to the kernel.

## Building
android-efi is built with [Meson]:

```
meson . build
ninja -C build
```

## Usage
Run `android.efi` from an UEFI Shell using the partition UUID and/or the path
to the boot image.

Alternatively, with [systemd-boot]:

```
title    Android
efi      /android.efi
options  normal 80868086-8086-8086-8086-000000000100
```

```
title    Recovery
efi      /android.efi
options  recovery 80868086-8086-8086-8086-000000000101
```

In this case, the first argument is ignored because this would be `android.efi`
when using the application in an UEFI shell.

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
[systemd-boot]: https://www.freedesktop.org/wiki/Software/systemd/systemd-boot/
