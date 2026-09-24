// Copyright (c) 2026 Herman van Hazendonk <github.com@herrie.org>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

/**
* @brief Minimal check macros shared by the PmLogLib tests.
*
* No test framework on purpose: these run on a device, where the only
* thing that can be relied on is a C library.
*
* @file test_util.h
**/

#ifndef PMLOG_TEST_UTIL_H
#define PMLOG_TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_failures++;                                                    \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                    \
    } while (0)

#define CHECK_EQ(actual, expected)                                           \
    do {                                                                     \
        long long a_ = (long long) (actual);                                 \
        long long e_ = (long long) (expected);                               \
        g_checks++;                                                          \
        if (a_ != e_) {                                                      \
            g_failures++;                                                    \
            fprintf(stderr, "FAIL %s:%d: %s == %lld, expected %lld\n",       \
                    __FILE__, __LINE__, #actual, a_, e_);                    \
        }                                                                    \
    } while (0)

#define CHECK_STR_EQ(actual, expected)                                       \
    do {                                                                     \
        const char* a_ = (actual);                                           \
        const char* e_ = (expected);                                         \
        g_checks++;                                                          \
        if ((a_ == NULL) || (e_ == NULL) || (strcmp(a_, e_) != 0)) {         \
            g_failures++;                                                    \
            fprintf(stderr, "FAIL %s:%d: %s == \"%s\", expected \"%s\"\n",   \
                    __FILE__, __LINE__, #actual,                             \
                    a_ ? a_ : "(null)", e_ ? e_ : "(null)");                 \
        }                                                                    \
    } while (0)

static inline int test_report(const char* name)
{
    printf("%s: %d checks, %d failures\n", name, g_checks, g_failures);
    return (g_failures == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif // PMLOG_TEST_UTIL_H
