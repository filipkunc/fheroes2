# Hero Specialties & Hi-Res Hero Portraits — Implementation Plan

Branch: `FK/Azure-Dragon` (or follow-up branch)
Status: planning, not started
Last updated: 2026-05-05

## Goal

Two related features, both editable in the Python sprite-editor tool at [tools/sprite_editor/](../tools/sprite_editor/):

1. **Hi-res hero portraits** — extend the existing monster RGBA pipeline to heroes (partial coverage; heroes without a hi-res asset fall back to the original ICN portrait).
2. **HoMM3-style hero specialties** — three kinds:
   - Spell boost (priority — implement first)
   - Unit boost
   - +1 specific resource per day

## Locked decisions

1. **Per-hero static** — specialty is fixed by hero ID (Solmyr always specializes in his thing). Not player-customizable.
2. **Codegen, not runtime JSON** — Python tool exports `src/fheroes2/heroes/heroes_specialty.cpp`; engine reads from a static table at compile time. No new game-data file path.
3. **Partial hi-res coverage** — start with a small set of heroes; the rest keep the original ICN portrait. Same fallback pattern as custom monsters.
4. **No save-format impact** — specialty is static-by-ID, like monster stats. If we ever want player-customizable specialties later, that becomes a save-format change and would need re-planning.

## Open questions to resolve before coding

- **Spell list scope** for the spell specialty dropdown: all spells, or only learnable ones? (Default: all.)
- **Resource specialty granularity**: fixed 7-option list (wood / ore / mercury / sulfur / crystal / gems / gold), with gold at a higher numeric amount (HoMM3 used +250/day)? Or free-form numeric input per resource?
- **Hi-res portrait sizes**: author one big PNG per hero and let `drawCustomPortraitInBox` downscale for medium/small? Or three separate authored sizes?

## Phasing

Each phase is independently shippable. Recommended order: do the **Python tool first** (since the user's anchor is the tool), then the C++ engine phases in order.

---

### Phase 0 — Python tool extension (start here)

**Files:**
- New: [tools/sprite_editor/resources/hero_manifest.json](../tools/sprite_editor/resources/hero_manifest.json)
- New: [tools/sprite_editor/widgets/specialty_panel.py](../tools/sprite_editor/widgets/specialty_panel.py)
- New: [tools/sprite_editor/models/hero_config.py](../tools/sprite_editor/models/hero_config.py) — parallel to `monster_config.py`
- New: [tools/sprite_editor/codegen/specialty_export.py](../tools/sprite_editor/codegen/specialty_export.py)
- Modified: [tools/sprite_editor/main_window.py](../tools/sprite_editor/main_window.py) — add Heroes tab/toggle in left dock
- Modified: [tools/sprite_editor/widgets/monster_list.py](../tools/sprite_editor/widgets/monster_list.py) — reuse for heroes, or fork into `entity_list.py`

**Tasks:**
1. Build `hero_manifest.json` with all ~70 vanilla heroes:
   ```json
   {
     "lord_kilburn": {
       "hero_id": 0,
       "race": "Knight",
       "display_name": "Lord Kilburn",
       "base_port_icn": "PORT0000",
       "prefix": "hero_lord_kilburn",
       "has_custom_sprites": false,
       "specialty": { "kind": "none" }
     }
   }
   ```
   The hero enum source: [src/fheroes2/heroes/heroes.h](../src/fheroes2/heroes/heroes.h) (enum starts ~line 88).
2. Heroes tab in the left dock — reuses the existing `monster_list` thumbnail-grid pattern. Selecting a hero loads it into the existing frame editor + a new specialty editor panel.
3. Specialty editor panel:
   - Kind dropdown: None / Spell / Unit / Resource
   - Per-kind payload form:
     - **Spell**: spell dropdown, damage bonus % (int), SP cost reduction (int)
     - **Unit**: monster dropdown, atk bonus / def bonus / speed bonus (int spinners)
     - **Resource**: resource dropdown (wood/ore/mercury/sulfur/crystal/gems/gold), amount/day (int)
   - Writes back to `hero_manifest.json` on change.
4. "Export specialties → C++" button → calls `codegen/specialty_export.py`, writes `src/fheroes2/heroes/heroes_specialty.cpp` (a single static table).

**Reuse:** [tools/sprite_editor/tools/icn_parser.py](../tools/sprite_editor/tools/icn_parser.py) already parses `PORT*` and `MINIPORT`. [tools/sprite_editor/models/sprite_data.py](../tools/sprite_editor/models/sprite_data.py) already handles `{prefix}_NNN.png` + `{prefix}_offsets.jsonl` — works for heroes unchanged.

---

### Phase 1 — Specialty data model + resource specialty (C++)

Smallest C++ change that exercises the whole specialty plumbing end-to-end. Resource bonus is the easiest to wire.

**Files:**
- Modified: [src/fheroes2/heroes/heroes.h](../src/fheroes2/heroes/heroes.h) — add `HeroSpecialty` struct + `Heroes::GetSpecialty()` decl
- New: `src/fheroes2/heroes/heroes_specialty.cpp` — codegen output (lookup table + getter)
- New: `src/fheroes2/heroes/heroes_specialty.h` — `HeroSpecialty` struct, `getHeroSpecialty(int heroId)` decl
- Modified: [src/fheroes2/kingdom/kingdom.cpp](../src/fheroes2/kingdom/kingdom.cpp) `Kingdom::GetIncome()` (lines 617-699) — `INCOME_HERO_SKILLS` branch
- Modified: hero info / meeting dialog (find via grep for the existing skill icon rendering)

**Struct shape:**
```cpp
struct HeroSpecialty {
    enum Kind { NONE, SPELL, UNIT, RESOURCE };
    Kind kind;
    union {
        struct { int spellId; int damageBonusPercent; int spCostReduction; } spell;
        struct { int monsterId; int atkBonus; int defBonus; int speedBonus; } unit;
        struct { int resourceId; int amountPerDay; } resource;
    };
};
```
(Or `std::variant` if the project uses C++17+ idioms — check existing code.)

**Tasks:**
1. Define struct + lookup table (codegen output of Phase 0).
2. `Heroes::GetSpecialty()` thin wrapper around the static table.
3. In `Kingdom::GetIncome` `INCOME_HERO_SKILLS` block (kingdom.cpp:657-662): iterate heroes, add specialty's resource delta to the relevant `Funds` field.
4. Show specialty in hero info dialog: small icon + tooltip. Look at existing skill-icon rendering for the visual pattern.

---

### Phase 2 — Spell specialty (priority)

**Files:**
- Modified: [src/fheroes2/spell/spell.cpp](../src/fheroes2/spell/spell.cpp) `Spell::spellPoints(HeroBase*)` — apply SP cost reduction
- Modified: battle spell-action code (find `Spell::Damage()` callers in `src/fheroes2/battle/`) — apply damage multiplier
- New helper: `getSpecialtyDamageMultiplier(hero, spell)` somewhere shared

**Tasks:**
1. SP cost: in `Spell::spellPoints(HeroBase*)`, if hero has matching spell specialty, subtract `spCostReduction` (clamped to ≥1).
2. Damage scaling: hook the battle-side damage application, **not** the static `Spell::Damage()` table. The actual application is where the spell is resolved against units. Multiply final damage by `(1 + damageBonusPercent/100)`.
3. **Defer for v1**: duration/area scaling for non-damage spells. Add only when needed.

---

### Phase 3 — Unit specialty

**Files:**
- Modified: [src/fheroes2/army/army_troop.cpp](../src/fheroes2/army/army_troop.cpp) `ArmyTroop::GetAttack/GetDefense` (lines 158-166)
- Modified: `src/fheroes2/battle/battle_troop.cpp` `Battle::Unit::GetSpeed`

**Tasks:**
1. In `ArmyTroop::GetAttack`/`GetDefense`: if commander has matching unit specialty, add bonus.
2. In `Battle::Unit::GetSpeed`: same pattern for speed bonus.
3. **Optional later**: HoMM3-style level-scaled bonus (grows with hero level). Recommend flat bonuses for v1, scale later if desired.

---

### Phase 4 — Hi-res hero portrait pipeline

Mostly mechanical copy-paste of the existing monster RGBA pipeline.

**Files:**
- Modified: [src/fheroes2/agg/agg_image.cpp](../src/fheroes2/agg/agg_image.cpp) — new `RGBAHeroEntry` registry alongside the existing `RGBACustomEntry registry[]`
- Modified: [src/fheroes2/agg/agg_image.h](../src/fheroes2/agg/agg_image.h) — new `AGG::GetRGBACustomHeroPortrait`, `AGG::renderHiResHeroPortrait` decls
- Modified: [src/fheroes2/heroes/heroes.cpp](../src/fheroes2/heroes/heroes.cpp) `Heroes::PortraitRedraw` (lines 2075-2126)
- Modified: hero meeting screen, army info, status-bar mini portrait, kingdom overview, hero list, adventure-map hero icon (find via grep for `PORT_BIG`/`PORT_MEDIUM`/`PORT_SMALL` callers and `MINIPORT` callers)
- New: `files/data/sprites/hero_{name}_000.png` + `hero_{name}_offsets.jsonl` per hero with hi-res art

**Tasks:**
1. Registry entry per hero with hi-res art: heroes likely need only 1 frame (static portrait), unlike monsters with 32-56 frames.
2. New funcs: `AGG::GetRGBACustomHeroPortrait(heroId, size)`, `AGG::renderHiResHeroPortrait(heroId, x, y, w, h)` — direct-blit to `Display::screenRGBA()`.
3. **Z-order rule** (same as monsters per [docs/RGBA_RENDERING_GUIDE.md](RGBA_RENDERING_GUIDE.md)): palette art first, RGBA overlay after. The widget code must always draw the palette portrait before calling the hi-res overlay so the WriteHook mirrors it before the RGBA write.
4. Three sizes (BIG/MEDIUM/SMALL): simplest to author one big PNG per hero and let `drawCustomPortraitInBox` downscale (already exists; respects `portrait_zoom` metadata).
5. Don't forget: `ArmyBar::RedrawItem` overrides — any subclass override needs the custom-portrait branch (see [src/fheroes2/heroes/heroes_meeting.cpp](../src/fheroes2/heroes/heroes_meeting.cpp) `MeetingArmyBar::RedrawItem` for the reference monster pattern; mirror it for heroes).

---

## Reference: existing patterns to copy

- **Custom monster registry**: `RGBACustomEntry registry[]` in `agg_image.cpp` — pattern to mirror for heroes.
- **Direct-blit Z-order**: see [docs/RGBA_RENDERING_GUIDE.md](RGBA_RENDERING_GUIDE.md) and the CLAUDE.md "Hi-res RGBA sprite pipeline" section.
- **Manifest-driven Python tool**: `monster_manifest.json` + `MonsterConfig` dataclass — clone for heroes.
- **Codegen target style**: look at how `monster_info.cpp` lays out its parallel arrays for inspiration on formatting the specialty table.

## Out of scope for v1

- Player-customizable specialties (would need save-format change).
- Specialty growth with hero level (HoMM3 unit-level scaling).
- Specialty for non-damage spell duration/area scaling.
- Animated hero portraits (use 1-frame static for now).
- Mass-redraw of all hero portraits — partial coverage only.

## Risks / things to watch

- **`ArmyBar::RedrawItem` overrides** — easy to miss a subclass and ship a half-painted UI. Search for all overrides before merging.
- **Codegen drift** — if anyone hand-edits `heroes_specialty.cpp`, the next Python export wipes it. Add a generated-file header comment warning against hand edits.
- **Hero ID stability** — the codegen table is keyed by hero enum value. If a hero is added in the middle of the enum upstream, IDs shift and the table breaks. Key by enum *name* in the JSON, resolve to ID at codegen time.
- **Spell/monster ID stability** — same concern for spell and unit specialty payloads. Store enum names in JSON, resolve at codegen.
