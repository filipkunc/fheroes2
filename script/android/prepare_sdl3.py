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

"""Prepare immutable SDL3 sources for both Java and CMake, outside the source tree."""

import json
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parents[2]
    revisions = json.loads((root / "cmake/sdl3-dependencies.json").read_text())
    destination = root / "android/build/sdl3-deps"
    destination.mkdir(parents=True, exist_ok=True)
    for name, revision in revisions.items():
        checkout = destination / name
        if not checkout.exists():
            subprocess.run(["git", "init", str(checkout)], check=True)
            subprocess.run(["git", "-C", str(checkout), "remote", "add", "origin",
                            f"https://github.com/libsdl-org/{name}.git"], check=True)
        if subprocess.check_output(["git", "-C", str(checkout), "status", "--porcelain"], text=True).strip():
            raise RuntimeError(f"Refusing to change modified dependency checkout: {checkout}")
        present = subprocess.run(["git", "-C", str(checkout), "cat-file", "-e", f"{revision}^{{commit}}"],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
        if not present:
            subprocess.run(["git", "-C", str(checkout), "fetch", "--depth=1", "origin", revision], check=True)
        subprocess.run(["git", "-C", str(checkout), "checkout", "--detach", revision], check=True)
        # Enabled mixer codecs are included in its tree; no optional codec submodules are required.
        print(f"{name}: {revision}", flush=True)


if __name__ == "__main__":
    main()
