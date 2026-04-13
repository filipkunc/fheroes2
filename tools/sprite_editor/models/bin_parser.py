"""Python port of the fheroes2 BIN file parser for monster animation data.

BIN files are exactly 821 bytes and contain animation frame sequences,
movement offsets, projectile data, and timing info for battle sprites.

Reference: src/fheroes2/agg/bin_info.cpp
"""

import struct
from dataclasses import dataclass, field
from enum import IntEnum
from pathlib import Path


CORRECT_FRM_LENGTH = 821


class AnimType(IntEnum):
    MOVE_START = 0
    MOVE_TILE_START = 1
    MOVE_MAIN = 2
    MOVE_TILE_END = 3
    MOVE_STOP = 4
    MOVE_ONE = 5
    TEMPORARY = 6
    STATIC = 7
    IDLE1 = 8
    IDLE2 = 9
    IDLE3 = 10
    IDLE4 = 11
    IDLE5 = 12
    DEATH = 13
    WINCE_UP = 14
    WINCE_END = 15
    ATTACK1 = 16
    ATTACK1_END = 17
    DOUBLEHEX1 = 18
    DOUBLEHEX1_END = 19
    ATTACK2 = 20
    ATTACK2_END = 21
    DOUBLEHEX2 = 22
    DOUBLEHEX2_END = 23
    ATTACK3 = 24
    ATTACK3_END = 25
    DOUBLEHEX3 = 26
    DOUBLEHEX3_END = 27
    SHOOT1 = 28
    SHOOT1_END = 29
    SHOOT2 = 30
    SHOOT2_END = 31
    SHOOT3 = 32
    SHOOT3_END = 33


# Human-friendly names for the animation dropdown
ANIM_DISPLAY_NAMES = {
    AnimType.MOVE_START: "Move Start",
    AnimType.MOVE_TILE_START: "Move Tile Start",
    AnimType.MOVE_MAIN: "Move Main",
    AnimType.MOVE_TILE_END: "Move Tile End",
    AnimType.MOVE_STOP: "Move Stop",
    AnimType.MOVE_ONE: "Move One Tile",
    AnimType.TEMPORARY: "Temporary",
    AnimType.STATIC: "Static",
    AnimType.IDLE1: "Idle 1",
    AnimType.IDLE2: "Idle 2",
    AnimType.IDLE3: "Idle 3",
    AnimType.IDLE4: "Idle 4",
    AnimType.IDLE5: "Idle 5",
    AnimType.DEATH: "Death",
    AnimType.WINCE_UP: "Wince Up",
    AnimType.WINCE_END: "Wince End",
    AnimType.ATTACK1: "Attack Top",
    AnimType.ATTACK1_END: "Attack Top End",
    AnimType.DOUBLEHEX1: "DoubleHex Top",
    AnimType.DOUBLEHEX1_END: "DoubleHex Top End",
    AnimType.ATTACK2: "Attack Front",
    AnimType.ATTACK2_END: "Attack Front End",
    AnimType.DOUBLEHEX2: "DoubleHex Front",
    AnimType.DOUBLEHEX2_END: "DoubleHex Front End",
    AnimType.ATTACK3: "Attack Bottom",
    AnimType.ATTACK3_END: "Attack Bottom End",
    AnimType.DOUBLEHEX3: "DoubleHex Bottom",
    AnimType.DOUBLEHEX3_END: "DoubleHex Bottom End",
    AnimType.SHOOT1: "Shoot Top",
    AnimType.SHOOT1_END: "Shoot Top End",
    AnimType.SHOOT2: "Shoot Front",
    AnimType.SHOOT2_END: "Shoot Front End",
    AnimType.SHOOT3: "Shoot Bottom",
    AnimType.SHOOT3_END: "Shoot Bottom End",
}

# Composite animations that combine start + main + end sequences
COMPOSITE_ANIMATIONS = {
    "Move": [AnimType.MOVE_START, AnimType.MOVE_MAIN, AnimType.MOVE_STOP],
    "Move (tile)": [AnimType.MOVE_TILE_START, AnimType.MOVE_MAIN, AnimType.MOVE_TILE_END],
    "Attack Top": [AnimType.ATTACK1, AnimType.ATTACK1_END],
    "Attack Front": [AnimType.ATTACK2, AnimType.ATTACK2_END],
    "Attack Bottom": [AnimType.ATTACK3, AnimType.ATTACK3_END],
    "Shoot Top": [AnimType.SHOOT1, AnimType.SHOOT1_END],
    "Shoot Front": [AnimType.SHOOT2, AnimType.SHOOT2_END],
    "Shoot Bottom": [AnimType.SHOOT3, AnimType.SHOOT3_END],
    "Death": [AnimType.DEATH],
    "Wince": [AnimType.WINCE_UP, AnimType.WINCE_END],
    "All Frames": [],  # special: plays every frame sequentially
}


@dataclass
class MonsterAnimInfo:
    eye_position: tuple[int, int] = (0, 0)
    frame_x_offsets: list[list[int]] = field(default_factory=list)  # 7 move types x 16 frames
    idle_count: int = 0
    idle_priorities: list[float] = field(default_factory=list)
    idle_delay: int = 0
    move_speed: int = 450
    shoot_speed: int = 0
    flight_speed: int = 0
    projectile_offsets: list[tuple[int, int]] = field(default_factory=list)
    projectile_angles: list[float] = field(default_factory=list)
    troop_count_offset_left: int = 0
    troop_count_offset_right: int = 0
    animation_frames: list[list[int]] = field(default_factory=list)  # 34 anim types

    def has_anim(self, anim_type: AnimType) -> bool:
        idx = int(anim_type)
        return idx < len(self.animation_frames) and len(self.animation_frames[idx]) > 0

    def get_frames(self, anim_type: AnimType) -> list[int]:
        idx = int(anim_type)
        if idx < len(self.animation_frames):
            return self.animation_frames[idx]
        return []

    def get_composite_frames(self, name: str) -> list[int]:
        """Get frame sequence for a composite animation (e.g. full move cycle)."""
        if name == "All Frames":
            # Collect all unique frame indices used across all animations
            all_frames = set()
            for frames in self.animation_frames:
                all_frames.update(frames)
            return sorted(all_frames) if all_frames else list(range(max(1, self.max_frame_index() + 1)))
        parts = COMPOSITE_ANIMATIONS.get(name, [])
        result = []
        for anim_type in parts:
            result.extend(self.get_frames(anim_type))
        return result

    def max_frame_index(self) -> int:
        """Return the highest frame index referenced by any animation."""
        max_idx = 0
        for frames in self.animation_frames:
            for f in frames:
                max_idx = max(max_idx, f)
        return max_idx

    def available_animations(self) -> list[str]:
        """Return list of composite animation names that have frames."""
        result = ["All Frames"]
        for name, parts in COMPOSITE_ANIMATIONS.items():
            if name == "All Frames":
                continue
            if any(self.has_anim(at) for at in parts):
                result.append(name)
        return result


def parse_bin(data: bytes) -> MonsterAnimInfo:
    """Parse a 821-byte BIN file into MonsterAnimInfo."""
    if len(data) != CORRECT_FRM_LENGTH:
        raise ValueError(f"BIN file must be {CORRECT_FRM_LENGTH} bytes, got {len(data)}")

    info = MonsterAnimInfo()

    # Eye position: bytes 1-4 (int16 LE each)
    info.eye_position = struct.unpack_from("<hh", data, 1)

    # Frame X offsets: 7 move types x 16 frames of int8
    for move_id in range(7):
        offsets = []
        for frame in range(16):
            val = struct.unpack_from("<b", data, 5 + move_id * 16 + frame)[0]
            offsets.append(val)
        info.frame_x_offsets.append(offsets)

    # Idle animation data
    info.idle_count = min(data[117], 5)
    for i in range(info.idle_count):
        info.idle_priorities.append(struct.unpack_from("<f", data, 118 + i * 4)[0])

    info.idle_delay = struct.unpack_from("<I", data, 158)[0]

    # Speed data
    info.move_speed = struct.unpack_from("<I", data, 162)[0]
    info.shoot_speed = struct.unpack_from("<I", data, 166)[0]
    info.flight_speed = struct.unpack_from("<I", data, 170)[0]

    # Projectile offsets: 3 pairs of int16
    for i in range(3):
        x = struct.unpack_from("<h", data, 174 + i * 4)[0]
        y = struct.unpack_from("<h", data, 176 + i * 4)[0]
        info.projectile_offsets.append((x, y))

    # Projectile angles
    projectile_count = min(data[186], 12)
    for i in range(projectile_count):
        info.projectile_angles.append(struct.unpack_from("<f", data, 187 + i * 4)[0])

    # Troop count offsets
    info.troop_count_offset_left = struct.unpack_from("<i", data, 235)[0]
    info.troop_count_offset_right = struct.unpack_from("<i", data, 239)[0]

    # Animation frame sequences: 34 types
    for idx in range(AnimType.MOVE_START, AnimType.SHOOT3_END + 1):
        count = min(data[243 + idx], 16)
        frames = []
        for frame in range(count):
            frames.append(data[277 + idx * 16 + frame])
        info.animation_frames.append(frames)

    return info


def load_bin_file(path: Path) -> MonsterAnimInfo:
    """Load and parse a BIN file from disk."""
    data = path.read_bytes()
    return parse_bin(data)
