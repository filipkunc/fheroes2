#!/usr/bin/env bash

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

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

blocked_files=()

check_path()
{
    local path="$1"
    local lowercase_path="${path,,}"

    case "${lowercase_path}" in
        *.agg|*.mp2|*.mx2|*.h2c|*.hxc|*.hs|*.smk|*.ogg)
            blocked_files+=( "${path}" )
            return
            ;;
    esac

    case "${lowercase_path}" in
        data/*|demo/*|homm2-data/*|original-game-data/*|android/app/src/main/assets/data/*)
            blocked_files+=( "${path}" )
            ;;
    esac
}

if (( $# > 0 )); then
    for path in "$@"; do
        check_path "${path}"
    done
else
    while IFS= read -r -d '' path; do
        check_path "${path}"
    done < <(git ls-files -z)
fi

if (( ${#blocked_files[@]} > 0 )); then
    echo "Original Heroes of Might and Magic II data must not be tracked:"
    printf '  %s\n' "${blocked_files[@]}"
    echo "Use an external local data path instead. Public CI must use synthetic fixtures."
    exit 1
fi

echo "Tracked asset guard passed."
