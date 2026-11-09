#!/usr/bin/env bash
#
# mayhem/build.sh — build RT-Thread's DHCP server (components/net/lwip-dhcpd/dhcp_server.c) into an
# AFL-instrumented, ASan+UBSan file-input fuzz target, plus the known-answer probe mayhem/test.sh runs.
# dhcp_server.c is compiled unmodified, against lwIP 2.1.2's headers; the RTOS and socket layer are
# stood in for by mayhem/dhcpd_fuzz-mined-cd92c3d/dhcpd_fuzz.c (see its header comment).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX

cd "$SRC"
LWIP="$SRC/components/net/lwip/lwip-2.1.2/src"
DHCPD="$SRC/components/net/lwip-dhcpd/dhcp_server.c"
HARNESS="$SRC/mayhem/dhcpd_fuzz-mined-cd92c3d/dhcpd_fuzz.c"
BUILD="$SRC/mayhem/build"
INCLUDES=(-I"$SRC/mayhem/dhcpd_port" -I"$LWIP/include")
LWIP_SRCS=("$LWIP/core/def.c" "$LWIP/core/ipv4/ip4_addr.c")
mkdir -p "$BUILD"

# dhcp_server.c shifts a byte left by 24 into an int (`*(opt + 2) << 24`, and the server address in the NAK
# check), which UBSan's shift-base reports on every request naming a 192.x.x.x address. It is benign on
# every supported target, so only that check is relaxed, only for this file.
DHCPD_RELAX=(-fno-sanitize=shift-base)

# build_dhcpd <out> <compiler> <extra flags...>: the harness binary at <out>.
build_dhcpd() {
  local out="$1" cc="$2"; shift 2
  local obj="$BUILD/$(basename "$out")" f
  rm -rf "$obj"; mkdir -p "$obj"
  for f in "${LWIP_SRCS[@]}" "$HARNESS"; do
    "$cc" -c -w -O1 "$@" "${INCLUDES[@]}" "$f" -o "$obj/$(basename "$f").o"
  done
  "$cc" -c -w -O1 "$@" "${DHCPD_RELAX[@]}" "${INCLUDES[@]}" "$DHCPD" -o "$obj/dhcp_server.c.o"
  "$cc" "$@" "$obj"/*.o ${EXTRA_OBJS:-} -o "$out"
}

# --- 1) AFL-instrumented, sanitized fuzz target -----------------------------------------------------
# LeakSanitizer off at build time (SPEC §6.2 item 15).
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$BUILD/lsan_off.o"
EXTRA_OBJS="$BUILD/lsan_off.o" build_dhcpd /mayhem/dhcpd_fuzz afl-clang-fast $SANITIZER_FLAGS $DEBUG_FLAGS

# --- 2) known-answer probe for test.sh: normal flags, NOT sanitized ---------------------------------
build_dhcpd /mayhem/dhcpd_kat "$CC" -DDHCPD_KAT
