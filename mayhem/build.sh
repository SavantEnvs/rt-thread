#!/usr/bin/env bash
#
# mayhem/build.sh — build RT-Thread's AT socket host-name lookup (components/net/at/at_socket/at_socket.c:
# at_gethostbyname, at_gethostbyname_r, at_getaddrinfo) for fuzzing, plus the known-answer probe that
# mayhem/test.sh runs. at_socket.c is compiled as is; mayhem/at_port supplies the host stand-ins for the
# RT-Thread kernel calls and the AT device (the at_device package is not in the tree).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}"
: "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE COVERAGE_FLAGS

cd "$SRC"
PORT="$SRC/mayhem/at_port"
BUILD="$SRC/mayhem/build"
AT="$SRC/components/net/at/at_socket/at_socket.c"
IPADDR="$SRC/components/net/netdev/src/netdev_ipaddr.c"
mkdir -p "$BUILD"

# DNS_MAX_NAME_LENGTH is lwIP's (256), which at_socket.c takes from lwip/opt.h in a full RT-Thread build.
INCS=(-DDNS_MAX_NAME_LENGTH=256 -I"$PORT" -I"$SRC/include" -I"$SRC/components/drivers/include"
  -I"$SRC/components/net/at/include" -I"$SRC/components/net/at/at_socket" -I"$SRC/components/net/netdev/include"
  -I"$SRC/components/net/sal/include" -I"$SRC/components/net/sal/include/socket"
  -I"$SRC/components/net/sal/include/socket/sys_socket")
SRCS=("$AT" "$IPADDR" "$PORT/at_port.c")

# LeakSanitizer off at build time (SPEC §6.2 item 15).
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$BUILD/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -x c -c "$STANDALONE_FUZZ_MAIN" -o "$BUILD/standalone_main.o"

# --- 1) sanitized, SanCov-instrumented library + harness -------------------------------------------
FUZZ_FLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -w -O1)
OBJS=()
for s in "${SRCS[@]}" "$PORT/fuzz_gethostbyname.c"; do
  obj="$BUILD/fuzz-$(basename "$s" .c).o"
  $CC "${FUZZ_FLAGS[@]}" "${INCS[@]}" -c "$s" -o "$obj"
  OBJS+=("$obj")
done
$CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "${OBJS[@]}" "$BUILD/lsan_off.o" -o /mayhem/at_gethostbyname
$CC $SANITIZER_FLAGS $DEBUG_FLAGS "${OBJS[@]}" "$BUILD/standalone_main.o" "$BUILD/lsan_off.o" -o /mayhem/at_gethostbyname-standalone

# --- 2) known-answer probe for test.sh: normal flags, NOT sanitized --------------------------------
$CC -O1 -w $COVERAGE_FLAGS "${INCS[@]}" "${SRCS[@]}" "$PORT/at_kat.c" -o /mayhem/at_kat
