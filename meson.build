# SPDX-License-Identifier: GPL-2.0
#
# android-efi
# Copyright (C) 2017 lambdadroid
#

project('android-efi', 'c',
    license: 'GPL-2.0',
    default_options: [
        'buildtype=debugoptimized',
        'c_std=gnu11',
        'warning_level=3',
        'werror=true'
    ]
)

arch = host_machine.cpu_family()

efi_include_dir = '/usr/include/efi'
efi_include = include_directories(
    efi_include_dir, join_paths(efi_include_dir, arch),
    is_system: true
)
efi_lds = '/usr/lib/elf_' + arch + '_efi.lds'
efi_crt = '/usr/lib/crt0-efi-' + arch + '.o'

# Splash screen
subdir('png2efi')
splash_src = png2efi.process('splash.png')

android_efi_lib = shared_library('android-efi',
    'main.c',
    'guid.c',
    'image.c',
    'android.c',
    'linux.c',
    'malloc.c',
    'graphics.c',
    'string.c',
    splash_src,

    include_directories: [efi_include],
    c_args: [
        '-fshort-wchar',
        '-ffreestanding',
        '-fno-strict-aliasing',
        '-fno-stack-protector',
        '-fno-stack-check',
        '-maccumulate-outgoing-args',
        '-mno-red-zone',
        '-mno-mmx',
        '-DGNU_EFI_USE_MS_ABI'
    ],
    objects: [efi_crt],
    link_args: [
        '-T', efi_lds,
        '-nostdlib',
        '-z', 'nocombreloc',
        '-Wl,-Bsymbolic',
        '-lefi', '-lgnuefi'
    ]
)

objcopy = find_program('objcopy')

custom_target('android.efi',
    output: 'android.efi',
    input: android_efi_lib,

    command: [objcopy,
        '--target=efi-app-' + arch,
        '-j', '.text',
        '-j', '.sdata',
        '-j', '.data',
        '-j', '.dynamic',
        '-j', '.dynsym',
        '-j', '.rel',
        '-j', '.rela',
        '-j', '.reloc',
        '@INPUT@', '@OUTPUT@'
    ],

    install: true,
    install_dir: ''
)
