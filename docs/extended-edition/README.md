# Extended Edition rebuild

This directory is the entry point for rebuilding the extended edition on a clean upstream base. It records decisions that should survive across development sessions and keeps the migration from depending on chat history.

## Starting a new development session

Read these files before changing code:

1. [ARCHITECTURE.md](ARCHITECTURE.md)
2. [TESTING.md](TESTING.md)
3. [PORTING_STATUS.md](PORTING_STATUS.md)

Then confirm the current branch and upstream state. Work only on the next unchecked item in `PORTING_STATUS.md`, unless that file is updated first to explain a change in direction.

At the end of a session, update `PORTING_STATUS.md` with:

- what was completed
- validation that was run
- new findings that affect later work
- the single best next step

## Project snapshot

This snapshot was recorded on 2026-08-18.

| Item | Value |
|---|---|
| Clean rebuild base | `ihhub/fheroes2` commit `495c790eceaded2f83b2850686796cc7b50b6586` |
| Preserved implementation | `filipkunc/fheroes2` branch `FK/Azure-Dragon` at `c9e6e8b406adb547b092295cc0b1408af608312f` |
| Primary platforms | Linux and Android |
| Product direction | A personal extended edition that regularly incorporates upstream fixes |
| Rendering direction | SDL3, full-color RGBA, physical-resolution rendering and painter ordering |
| Artwork direction | Full-color artwork designed primarily for sharp 2x and 3x output |

The preserved branch is reference material. Do not rewrite or force-update it. Port coherent features into new topic branches instead of rebasing its full history.

## Agreed scope

The extended edition retains:

- custom creatures
- high-resolution custom artwork
- hero specialties
- the MP2/MX2 importer
- the SDL3 and RGBA rendering direction

The art generation and editing application will become a separate project. The game build must consume already accepted runtime assets and must never depend on an AI service.

## Non-negotiable data rule

Original Heroes of Might and Magic II data is not part of this repository. This includes data from purchased releases and the downloadable demo.

Public CI must work from a clean checkout without Ubisoft assets. It must not download, commit, render into published screenshots, or upload original game artwork as an artifact. See [TESTING.md](TESTING.md) for the synthetic-data strategy.

