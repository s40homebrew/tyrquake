#!/usr/bin/env bash
set -euo pipefail
P=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
S40_SDK_ROOT=${S40_SDK_ROOT:-$(CDPATH='' cd -- "$P/../.." && pwd)/main}
[[ -f "$S40_SDK_ROOT/tools/env.sh" ]] || { echo "Set S40_SDK_ROOT to the S40 homebrew source root" >&2; exit 1; }
source "$S40_SDK_ROOT/tools/env.sh"
export S40_SDK_ROOT
R="$S40_SDK_ROOT"
B="$P/build"
UPSTREAM="$P/../common"
S40_OPT_LEVEL=${S40_OPT_LEVEL:-3}
[[ "$S40_OPT_LEVEL" == 2 || "$S40_OPT_LEVEL" == 3 ]] || { echo "S40_OPT_LEVEL must be 2 or 3"; exit 1; }
S40_RENDER_LTO=${S40_RENDER_LTO:-1}
[[ "$S40_RENDER_LTO" == 0 || "$S40_RENDER_LTO" == 1 ]] || { echo "S40_RENDER_LTO must be 0 or 1"; exit 1; }
S40_FIXED_SPANS=${S40_FIXED_SPANS:-1}
[[ "$S40_FIXED_SPANS" == 0 || "$S40_FIXED_SPANS" == 1 ]] || { echo "S40_FIXED_SPANS must be 0 or 1"; exit 1; }
ARM_GCC=${ARM_GCC:-$(command -v arm-none-eabi-gcc)}
for t in "$ARM_GCC" "$CC" "$LD" "$LLVM/llvm-objcopy" "$JAVA8_HOME/bin/java"; do
    test -x "$t" || { printf 'Missing tool: %s\n' "$t"; exit 1; }
done
python3 "$R/tools/clean_build_outputs.py" java "$P" "$B" build "$P"
mkdir -p "$B/obj" "$B/classes" "$B/preverified" "$P/dist"
# Compile the included engine source, including the S40 adaptations.
python3 "$P/scripts/gen_imports.py"
python3 "$P/scripts/gen_carrier.py"
# Firmware type_15_defines identifies ARM1176JZ-S. Tune scheduling for it,
# retaining ARMv6 instructions and software float (no VFP/NEON dependency).
FLAGS=(-std=gnu99 -target arm-none-eabi -march=armv6 -mtune=arm1176jz-s -marm -mfloat-abi=soft -O"$S40_OPT_LEVEL" -fPIC -fvisibility=hidden -ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-strict-aliasing -fwrapv -fno-common -ffunction-sections -fdata-sections -Wno-incompatible-pointer-types-discards-qualifiers -Wno-pointer-sign -Wno-deprecated-non-prototype -I"$P/native/include" -I"$P/native" -I"$UPSTREAM" -DS40 -DNQ_HACK -D__LIBRETRO__ -DHAVE_STRINGS_H -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DTYR_VERSION=0.62 -I"$UPSTREAM/../libretro-common/include")
# Keep useful workspace-relative filenames in every native compilation,
# including future debug builds, without exposing the builder's home path.
FLAGS+=("-ffile-prefix-map=$R=." "-ffile-prefix-map=$P/..=." "-ffile-prefix-map=$P=.")
FLAGS+=("-DS40_FIXED_SPANS=$S40_FIXED_SPANS")
FLAGS+=("-I$R/homebrew/include" "-I$R/homebrew/src/native")
# Same portable engine source list as upstream, with the host system backend
# replaced by quake_s40.c. audio_s40.c connects the original sound mixer;
# ui_s40.c adapts original UI drawing while keeping small text at native size.
SOURCES=$(python3 - "$UPSTREAM/../Makefile.common" <<'PYLIST'
import re,sys
s=open(sys.argv[1]).read().split('ifneq ($(STATIC_LINKING)')[0]
names=re.findall(r'\$\(CORE_DIR\)/common/(\w+\.c)',s)
sound={'snd_dma.c','snd_mem.c','snd_mix.c','snd_codec.c'}
print(' '.join(n for n in names if (not n.startswith(('snd_', 'cd_')) or n in sound) and n not in ('libretro.c','bgmusic.c'))+' net_none.c')
PYLIST
)
OBJECTS=()
printf 'Compiling freestanding ARMv6 TyrQuake...\n'
for s in $SOURCES; do
    RENDER_FLAGS=()
    # Optimize across renderer/math files while keeping engine/platform
    # boundaries separately compiled. No relaxed floating-point semantics.
    case "$s" in
        d_*.c|r_*.c|mathlib.c)
            if [[ "$S40_RENDER_LTO" == 1 ]]; then RENDER_FLAGS=(-flto); fi ;;
    esac
    "$CC" "${FLAGS[@]}" "${RENDER_FLAGS[@]}" -include retro_inline.h -include strings.h -c "$UPSTREAM/$s" -o "$B/obj/${s%.c}.o"
    OBJECTS+=("$B/obj/${s%.c}.o")
done
for s in libc quake_s40 newlib_support retro_s40 audio_s40 music_null ui_s40 span_s40; do
    "$CC" "${FLAGS[@]}" -include retro_inline.h -include strings.h -c "$P/native/$s.c" -o "$B/obj/$s.o"
    OBJECTS+=("$B/obj/$s.o")
done
for s in "$R/homebrew/src/native/audio/core.c" "$R/homebrew/src/native/audio/commgr/driver.c" "$R/homebrew/src/native/audio/devices/rm839_0233.c"; do
    object="$B/obj/shared_$(basename "${s%.c}").o"
    "$CC" "${FLAGS[@]}" -c "$s" -o "$object"
    OBJECTS+=("$object")
done
"$CC" "${FLAGS[@]}" -c "$B/imports.S" -o "$B/obj/imports.o"
# Use the generic ARMv4T soft-float libraries: ARM11-compatible, with no
# MOVW/MOVT absolute relocations. All library data relocates with quake.bin.
LIBC=$("$ARM_GCC" -print-file-name=libc.a)
LIBM=$("$ARM_GCC" -print-file-name=libm.a)
LIBGCC=$("$ARM_GCC" -print-file-name=libgcc.a)
"$LD" -shared -Bsymbolic --no-undefined --gc-sections --exclude-libs=ALL -e native_main --hash-style=sysv -z max-page-size=4096 -Map="$B/quake.map" -o "$B/quake.elf" "${OBJECTS[@]}" "$B/obj/imports.o" --start-group "$LIBM" "$LIBC" "$LIBGCC" --end-group -z notext
python3 "$P/scripts/pack.py"
"$LLVM/llvm-readobj" --file-headers --program-headers --relocations "$B/quake.elf" > "$B/quake.elf.txt"
printf 'Building bootstrap loader and CLDC launcher...\n'
"$CC" "${FLAGS[@]}" -Os -c "$P/native/loader.c" -o "$B/loader-c.o"
"$CC" "${FLAGS[@]}" -c "$P/native/loader.S" -o "$B/loader-s.o"
"$LD" -T "$P/native/loader.ld" -o "$B/loader.elf" "$B/loader-s.o" "$B/loader-c.o"
"$LLVM/llvm-objcopy" -O binary "$B/loader.elf" "$B/native.bin"
"$LLVM/llvm-objdump" -d "$B/loader.elf" > "$B/loader.disasm.txt"
"$JAVA8_HOME/bin/java" -cp "$LEGACY_JAVAC" j2me.thirdparty.legacyjavac.Main -source 1.3 -target 1.1 -g:none -bootclasspath "$SDK_CLASSES" -d "$B/classes" "$B/QuakeLauncher.java"
python3 "$R/homebrew/tools/preverify.py" "$JAVA8_HOME/bin/java" "$PROGUARD_JAR" "$SDK_CLASSES" "$B/classes" "$B/preverified"
"$JAVA8_HOME/bin/javap" -classpath "$B/preverified" -c -p -v QuakeLauncher > "$B/QuakeLauncher.javap.txt"
cp "$B/native.bin" "$B/quake.bin" "$B/preverified/"
python3 - "$P" <<'PY'
from pathlib import Path
import sys, zipfile
p=Path(sys.argv[1]);d=p/'dist';manifest=(p/'src/MANIFEST.MF').read_text().rstrip()+'\n\n'
# Write the supplied manifest verbatim with JAR-required CRLF endings. The
# JDK jar tool would append Created-By with the local JDK version and vendor.
# Use the ZIP epoch for every entry, including the manifest. ZipFile.write()
# copies filesystem mtimes; writestr(name, ...) uses the current local time.
zip_epoch=(1980,1,1,0,0,0)
with zipfile.ZipFile(d/'RM839NativeQuake.jar','w',zipfile.ZIP_DEFLATED) as archive:
    def write_entry(name,data):
        entry=zipfile.ZipInfo(name,date_time=zip_epoch)
        entry.compress_type=zipfile.ZIP_DEFLATED
        entry.external_attr=0o100644 << 16
        archive.writestr(entry,data)
    write_entry('META-INF/MANIFEST.MF',manifest.replace('\n','\r\n').encode())
    write_entry('licenses/COPYING.QUAKE',(p/'COPYING').read_bytes())
    for license_file in sorted((p/'licenses').iterdir()):
        if license_file.is_file():
            write_entry('licenses/'+license_file.name,license_file.read_bytes())
    classes=p/'build/preverified'
    for path in sorted(classes.rglob('*')):
        if path.is_file():
            name=path.relative_to(classes).as_posix()
            if name=='META-INF/MANIFEST.MF':
                raise SystemExit('Unexpected manifest in preverified output')
            write_entry(name,path.read_bytes())
m=manifest.replace('Manifest-Version: 1.0\n','').strip()
# Inspect decompressed entries: scanning the compressed JAR itself misses
# embedded paths. Reject macOS/Linux home paths and this workspace's root.
with zipfile.ZipFile(d/'RM839NativeQuake.jar') as archive:
    for entry in archive.infolist():
        if entry.date_time!=zip_epoch or entry.extra:
            raise SystemExit('Unexpected ZIP timestamp/extra metadata: '+entry.filename)
        data=archive.read(entry)
        if any(prefix in data for prefix in (str(p).encode(), b'/Users/', b'/home/')):
            raise SystemExit('Host path leaked into JAR entry: '+entry.filename)
m+='\nMIDlet-Jar-URL: RM839NativeQuake.jar\nMIDlet-Jar-Size: '+str((d/'RM839NativeQuake.jar').stat().st_size)+'\n'
(d/'RM839NativeQuake.jad').write_bytes(m.replace('\n','\r\n').encode())
print('Built',d/'RM839NativeQuake.jar')
PY

python3 "$P/scripts/verify.py"
cp "$P/INSTALL.txt" "$P/dist/RM839Quake-README.txt"
