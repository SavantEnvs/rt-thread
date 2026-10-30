#!/usr/bin/env bash
#
# mayhem/build.sh — build RT-Thread's in-tree lwIP fuzz harnesses (lwIP 2.0.3 / 2.1.2 / 2.2.1,
# components/net/lwip/lwip-*/test/fuzz) as AFL-instrumented, ASan+UBSan file-input targets, plus the
# lwIP known-answer probe that mayhem/test.sh runs. The unix `contrib` port the upstream fuzz Makefile
# expects is not in the tree, so mayhem/lwip_port provides the (NO_SYS) arch glue instead.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
LWIP_ROOT="$SRC/components/net/lwip"
PORT="$SRC/mayhem/lwip_port"
BUILD="$SRC/mayhem/build"
mkdir -p "$BUILD"

# Prints the lwIP source files to build for <version> (library objects, no sys_arch/port).
lwip_sources() {
  local ver="$1" lwipdir="$LWIP_ROOT/lwip-$1/src" vars
  case "$ver" in
    2.0.3) vars='$(LWIPNOAPPSFILES)' ;;
    *)     vars='$(LWIPNOAPPSFILES) $(SNMPFILES) $(HTTPFILES) $(LWIPERFFILES) $(MDNSFILES)' ;;
  esac
  make -s -f - <<MK
LWIPDIR=$lwipdir
include $lwipdir/Filelists.mk
all: ; @echo $vars
MK
}

# lwIP 2.1.2's default MEMP_NUM_SYS_TIMEOUT only covers the core stack; the harness also starts the httpd,
# lwiperf, mdns and snmp apps, which register their own timeouts. Size the pool for them as lwIP 2.2.1's
# upstream fuzz lwipopts.h does (17). 2.2.1 already sets it; 2.0.3 builds no apps.
ver_defs() { [ "$1" != 2.1.2 ] || echo "-DMEMP_NUM_SYS_TIMEOUT=17"; }

# build_lwip <version> <out dir> <compiler> <flags...>: static libwip.a for one lwIP version.
build_lwip() {
  local ver="$1" out="$2" cc="$3"; shift 3
  local fuzzdir="$LWIP_ROOT/lwip-$ver/test/fuzz"
  mkdir -p "$out"
  rm -f "$out"/*.o "$out/liblwip.a"
  lwip_sources "$ver" | tr ' ' '\n' | grep '\.c$' | \
    xargs -P "$MAYHEM_JOBS" -I{} bash -c '
      src="$1"; shift; cc="$1"; shift; out="$1"; shift
      obj="$out/$(echo "${src#*/lwip-}" | tr "/" "_").o"
      "$cc" -c "$@" "$src" -o "$obj"' _ {} "$cc" "$out" \
      -w -O1 "$@" $(ver_defs "$ver") -I"$LWIP_ROOT/lwip-$ver/src/include" -I"$PORT" -I"$fuzzdir"
  "$cc" -c -w -O1 "$@" $(ver_defs "$ver") -I"$LWIP_ROOT/lwip-$ver/src/include" -I"$PORT" -I"$fuzzdir" "$PORT/sys_arch.c" -o "$out/port_sys_arch.o"
  ar rcs "$out/liblwip.a" "$out"/*.o
}

FUZZ_FLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS)

# LeakSanitizer off at build time (SPEC §6.2 item 15).
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$BUILD/lsan_off.o"

# --- 1) AFL-instrumented, sanitized fuzz targets ---------------------------------------------------
FUZZ_CC=afl-clang-fast
# <target name>:<lwip version>:<harness .c files>
TARGETS=(
  "lwip_fuzz_2_2_1:2.2.1:fuzz.c"
  "lwip_fuzz2_2_2_1:2.2.1:fuzz2.c"
  "lwip_fuzz3_2_2_1:2.2.1:fuzz3.c"
  "lwip_fuzz_2_1_2:2.1.2:fuzz.c"
  "lwip_fuzz_2_0_3:2.0.3:fuzz.c"
)
for ver in 2.2.1 2.1.2 2.0.3; do
  build_lwip "$ver" "$BUILD/fuzz-$ver" "$FUZZ_CC" "${FUZZ_FLAGS[@]}"
done
for t in "${TARGETS[@]}"; do
  IFS=: read -r name ver harness <<<"$t"
  fuzzdir="$LWIP_ROOT/lwip-$ver/test/fuzz"
  extra=()
  [ ! -f "$fuzzdir/fuzz_common.c" ] || extra+=("$fuzzdir/fuzz_common.c")
  "$FUZZ_CC" "${FUZZ_FLAGS[@]}" -w -O1 $(ver_defs "$ver") \
    -I"$LWIP_ROOT/lwip-$ver/src/include" -I"$PORT" -I"$fuzzdir" \
    "$fuzzdir/$harness" "${extra[@]}" "$BUILD/lsan_off.o" "$BUILD/fuzz-$ver/liblwip.a" -lm \
    -o "/mayhem/$name"
done

# --- 2) known-answer probe for test.sh: normal flags, NOT sanitized -------------------------------
build_lwip 2.2.1 "$BUILD/kat-2.2.1" "$CC" $COVERAGE_FLAGS
$CC -O1 $COVERAGE_FLAGS -w \
  -I"$LWIP_ROOT/lwip-2.2.1/src/include" -I"$PORT" -I"$LWIP_ROOT/lwip-2.2.1/test/fuzz" \
  "$SRC/mayhem/kat/lwip_kat.c" "$BUILD/kat-2.2.1/liblwip.a" -lm -o /mayhem/lwip_kat
