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

project(PmLogLibTests C)

# Unset the inherited flags for the same reason cxx/ does: the library's
# --version-script has no business on a test executable. The ones the
# tests do want are restored below.
unset(CMAKE_C_FLAGS)

webos_include_install_paths()
webos_add_compiler_flags(ALL C --std=gnu23)
webos_add_compiler_flags(ALL -Wall -Wextra)
webos_add_linker_options(ALL --no-undefined)

find_package(Threads REQUIRED)

set(PMLOG_TESTS
	test_contexts
	test_sharing
	test_stress
)

foreach(test ${PMLOG_TESTS})
	add_executable(${test} ${test}.c)
	target_link_libraries(${test} ${CMAKE_PROJECT_NAME} Threads::Threads)

	# run-tests.sh gives each test a private /dev/shm where it can, so a
	# run neither depends on nor disturbs the contexts of the machine it
	# runs on
	add_test(NAME ${test}
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run-tests.sh $<TARGET_FILE:${test}>)
endforeach()
