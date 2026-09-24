#!/bin/sh
# Copyright (c) 2026 Herman van Hazendonk <github.com@herrie.org>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# Run whichever static analysers are installed over src/PmLogLib.c.
#
# Needs a configured build tree for the generated PmLogLib.h and for the
# include paths of glib and pbnjson.
#
# Usage: run-static-checks.sh [build directory]   (default: ./build)

set -u

src_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${1:-$src_dir/build}

generated=$(find "$build_dir" -name PmLogLib.h -print 2>/dev/null | head -1)
if [ -z "$generated" ]; then
	echo "no generated PmLogLib.h under $build_dir - configure a build first" >&2
	echo "  cmake -S $src_dir -B $build_dir" >&2
	exit 2
fi

includes="-I$(dirname "$generated") -I$src_dir/include/private"
for package in glib-2.0 pbnjson_c; do
	if pkg-config --exists "$package" 2>/dev/null; then
		includes="$includes $(pkg-config --cflags-only-I "$package")"
	fi
done

# The install path macros come from the build system; the analysers only
# need them to be strings.
defines="-DWEBOS_INSTALL_SYSCONFDIR=\"/etc\" \
-DWEBOS_INSTALL_PREFERENCESDIR=\"/var/preferences\" \
-DWEBOS_INSTALL_LOGDIR=\"/var/log\""

source_file=$src_dir/src/PmLogLib.c
status=0
ran=0

run_check()
{
	name=$1
	shift

	echo "=== $name"
	if "$@"; then
		:
	else
		echo "=== $name: reported problems"
		status=1
	fi
	ran=$((ran + 1))
}

if command -v gcc >/dev/null 2>&1; then
	# shellcheck disable=SC2086
	run_check "gcc -fanalyzer" sh -c "gcc -fsyntax-only -std=gnu23 -Wall -Wextra \
		-Wformat-signedness -fanalyzer $defines $includes '$source_file' 2>&1 | tee /dev/stderr | \
		grep -q . && exit 1 || exit 0"
fi

if command -v clang >/dev/null 2>&1; then
	# Annex K's *_s functions do not exist in glibc, so that checker only
	# produces noise here.
	# shellcheck disable=SC2086
	run_check "clang --analyze" sh -c "clang --analyze -std=gnu23 \
		-Xclang -analyzer-disable-checker=security.insecureAPI.DeprecatedOrUnsafeBufferHandling \
		$defines $includes '$source_file' 2>&1 | tee /dev/stderr | grep -q . && exit 1 || exit 0"
fi

if command -v clang-tidy >/dev/null 2>&1; then
	# shellcheck disable=SC2086
	run_check "clang-tidy" sh -c "clang-tidy --quiet \
		--checks='-*,bugprone-*,cert-*,clang-analyzer-*,concurrency-*,performance-*,portability-*,android-cloexec-*,-bugprone-easily-swappable-parameters,-bugprone-reserved-identifier,-cert-dcl37-c,-cert-dcl51-cpp,-cert-err33-c,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling' \
		'$source_file' -- -std=gnu23 $defines $includes 2>&1 | grep -E 'warning:|error:' && exit 1 || exit 0"
fi

if command -v cppcheck >/dev/null 2>&1; then
	# shellcheck disable=SC2086
	run_check "cppcheck" sh -c "cppcheck --enable=warning,performance,portability \
		--std=c23 --inline-suppr --suppress=missingIncludeSystem --suppress=checkersReport \
		--error-exitcode=1 $defines $includes '$source_file'"
fi

if command -v sparse >/dev/null 2>&1; then
	# sparse does not parse C23 yet, so give it a copy with the two C23
	# spellings this file uses rewritten in their C17 form.
	c17_copy=$(mktemp -t PmLogLib.XXXXXX.c)
	sed 's/\[\[maybe_unused\]\] //; s/\bconstexpr\b/const/g' "$source_file" > "$c17_copy"
	# shellcheck disable=SC2086
	run_check "sparse" sh -c "sparse -Wall -std=gnu17 $defines $includes '$c17_copy' 2>&1 | \
		grep -v 'Variable length array' | tee /dev/stderr | grep -q . && exit 1 || exit 0"
	rm -f "$c17_copy"
fi

if [ "$ran" -eq 0 ]; then
	echo "no analysers found (gcc, clang, clang-tidy, cppcheck, sparse)" >&2
	exit 2
fi

exit "$status"
