#!/usr/bin/env python3

###########################################################################
#   fheroes2: https://github.com/ihhub/fheroes2                           #
#   Copyright (C) 2026                                                    #
#                                                                         #
#   This program is free software; you can redistribute it and/or modify  #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 2 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   This program is distributed in the hope that it will be useful,       #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program; if not, write to the                         #
#   Free Software Foundation, Inc.,                                       #
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             #
###########################################################################

"""Validate the native SDL3 APK and the complete staged asset digest without game data."""

import argparse
import hashlib
from pathlib import PurePosixPath
import struct
import zipfile


def check_apk(path, abis):
    machines = {"arm64-v8a": 183, "armeabi-v7a": 40, "x86": 3, "x86_64": 62}
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError("APK has duplicate entries")
        libraries = {name for name in names if name.startswith("lib/") and name.endswith(".so")}
        expected = {f"lib/{abi}/{library}" for abi in abis
                    for library in ("libmain.so", "libSDL3.so", "libSDL3_mixer.so", "libc++_shared.so")}
        if libraries != expected:
            raise ValueError(f"Unexpected native packaging: missing={expected - libraries}, extra={libraries - expected}")
        for name in sorted(libraries):
            data = archive.read(name)
            abi = name.split("/")[1]
            if data[:4] != b"\x7fELF" or data[5] != 1 or struct.unpack_from("<H", data, 18)[0] != machines[abi]:
                raise ValueError(f"Wrong ELF architecture: {name}")
        assets = {name.removeprefix("assets/"): archive.read(name) for name in names
                  if name.startswith("assets/") and not name.endswith("/") and name != "assets/assets.digest"}
        digest = []
        for name, data in sorted(assets.items()):
            if PurePosixPath(name).suffix.lower() in {".agg", ".mp2", ".mx2", ".smk"}:
                raise ValueError(f"Original game data must not be packaged: {name}")
            digest.append(f"{hashlib.sha512(data).hexdigest()} {len(data): 12d} {name}\n")
        if archive.read("assets/assets.digest").decode("utf-8") != "".join(digest):
            raise ValueError("Packaged assets do not match their deterministic digest")
        if not any(name.startswith("maps/") for name in assets) or not any(name.startswith("files/data/") for name in assets):
            raise ValueError("Missing game-owned maps or engine resources")
    print(f"SDL3 APK verified: {', '.join(abis)}; {len(assets)} assets with matching SHA-512 digests")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk")
    parser.add_argument("abis", nargs="+", choices=("arm64-v8a", "armeabi-v7a", "x86", "x86_64"))
    args = parser.parse_args()
    check_apk(args.apk, args.abis)


if __name__ == "__main__":
    main()
