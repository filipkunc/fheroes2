# Idempotently create a symbolic link at LINK_DEST pointing to LINK_SOURCE.
#
# Used as a post-build step so the binary in $<TARGET_FILE_DIR:fheroes2> can
# resolve <root>/maps (and similar dev assets) without the developer having to
# place files manually next to every build configuration.
#
# Behaviour:
#   - If LINK_DEST does not exist: create the symlink. On Windows the user must
#     either be in Developer Mode or have SeCreateSymbolicLinkPrivilege; if the
#     symlink call fails, file(CREATE_LINK ... COPY_ON_ERROR) falls back to a
#     directory copy, which still produces a working <root>/maps.
#   - If LINK_DEST is already a symlink: replace it (the source target may have
#     moved between configurations).
#   - If LINK_DEST is a real directory: leave it alone. Treat its existence as
#     an explicit override by the developer (e.g. a hand-placed campaign maps
#     folder or a manual symlink to a non-default location).

# CMP0205 NEW: file(CREATE_LINK ... COPY_ON_ERROR) recursively copies a directory
# source instead of failing — required for the dev symlink fallback to work
# silently when symlink creation is not permitted (e.g. no Developer Mode).
cmake_policy(SET CMP0205 NEW)

if(NOT DEFINED LINK_SOURCE OR NOT DEFINED LINK_DEST)
    message(FATAL_ERROR "dev_symlink.cmake requires -DLINK_SOURCE=... -DLINK_DEST=...")
endif()

# Source missing — nothing to link to. Skip silently rather than error so a
# dev who only has e.g. maps/ but no anim/ at the project root doesn't see
# build failures.
if(NOT EXISTS "${LINK_SOURCE}")
    return()
endif()

if(EXISTS "${LINK_DEST}")
    if(IS_SYMLINK "${LINK_DEST}")
        file(REMOVE "${LINK_DEST}")
    else()
        # Real directory placed by the developer — don't touch it.
        return()
    endif()
endif()

file(CREATE_LINK "${LINK_SOURCE}" "${LINK_DEST}" SYMBOLIC COPY_ON_ERROR)
