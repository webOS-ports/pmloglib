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
# Run PmLogLib test programs with a private /dev/shm.
#
# The context table is a system wide object: a test run would otherwise
# inherit whatever contexts the machine already has and leave its own
# behind. Where the kernel allows an unprivileged mount namespace we give
# each test a fresh tmpfs instead; where it does not, we say so and run
# anyway.
#
# Usage: run-tests.sh <test binary> [<test binary> ...]

set -u

if [ "$#" -eq 0 ]; then
	echo "usage: $0 <test binary> [<test binary> ...]" >&2
	exit 2
fi

if [ "${PMLOG_TEST_ISOLATED:-0}" != "1" ]; then
	if unshare --mount --map-root-user true 2>/dev/null; then
		PMLOG_TEST_ISOLATED=1
		export PMLOG_TEST_ISOLATED
		exec unshare --mount --map-root-user "$0" "$@"
	fi

	echo "run-tests.sh: no private mount namespace available; the tests"
	echo "run-tests.sh: share /dev/shm with the rest of the system"
fi

if [ "${PMLOG_TEST_ISOLATED:-0}" = "1" ]; then
	mount -t tmpfs tmpfs /dev/shm || exit 1
fi

failed=0

for test_binary in "$@"; do
	if [ "${PMLOG_TEST_ISOLATED:-0}" = "1" ]; then
		# a clean table for every test
		rm -f /dev/shm/pmloglib.shm /dev/shm/pmloglib.lock
	fi

	echo "--- $(basename "$test_binary")"
	if "$test_binary"; then
		:
	else
		echo "--- $(basename "$test_binary"): FAILED (exit $?)"
		failed=$((failed + 1))
	fi
done

if [ "$failed" -ne 0 ]; then
	echo "$failed test program(s) failed"
	exit 1
fi

exit 0
