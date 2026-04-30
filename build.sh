#!/bin/zsh

source edksetup.sh
build -a X64 -t GCC -b DEBUG -p MetaXPkg/MetaXPkg.dsc
BaseTools/Source/C/bin/EfiRom -f 0x9999 -i 0x0001 -e Build/MetaXPkg/DEBUG_GCC/X64/MetaXGopDxe.efi -o Build/MetaXPkg/DEBUG_GCC/X64/MetaXGopDxe.rom
