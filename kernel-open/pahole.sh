#!/bin/sh

if [ "$KBUILD_VERBOSE" = "1" ]; then
    set -x
fi

PAHOLE=${ORIG_PAHOLE:-pahole}
# Use DRY_RUN=echo to print the command instead of executing it, for debugging.
DRY_RUN=${DRY_RUN:-}

btf_encode=0
lang_exclude=

# Parse args: detect BTF encoding and existing --lang_exclude, while
# rebuilding the argument list without --lang_exclude.
args_start=0
for arg in "$@"; do
    case "$arg" in
    --btf_features=*encode_force* | -J | --btf_encode | --btf_encode_force)
        btf_encode=1
        ;;
    --lang_exclude=*)
        lang_exclude="${arg#*=}"
        # Drop this arg from the rebuilt list.
        if [ $args_start -eq 0 ]; then
            set --
            args_start=1
        fi
        continue
        ;;
    esac
    if [ $args_start -eq 0 ]; then
        set -- "$arg"
        args_start=1
    else
        set -- "$@" "$arg"
    fi
done

# Capture anything the kernel is already excluding and extend it with c++.
# Even if in future the kernel doesn't have to exclude other languages,
# make sure to exclude c++ when generating encoding BTF.
if [ $btf_encode -eq 1 ] || [ -n "$lang_exclude" ]; then
    if [ -n "$lang_exclude" ]; then
        lang_exclude="${lang_exclude},c++"
    else
        lang_exclude="c++"
    fi
    set -- --lang_exclude="$lang_exclude" "$@"

    $DRY_RUN "$PAHOLE" "$@"
    ret=$?

    # Until pahole 1.31, return code was still an error regardless of
    # --btf_encode_force / --btf_features=encode_force.
    # A pure version check is not possible as it's likely that pahole
    # from distros will have fixes backported. Just ignore the error for now.
    if [ $ret -ne 0 ]; then
        echo "pahole.sh: warning: pahole exited with status $ret (ignored)" >&2
    fi
    exit 0
fi

# Do not override options or exit status, as this may interfere with the kernel
# build system when determining pahole version and options.
$DRY_RUN exec "$PAHOLE" "$@"
