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
* @brief Context table and level handling.
*
* @file test_contexts.c
**/

#include "test_util.h"

#include <PmLogLib.h>

// the level constants are deliberately deprecated in the public header
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static void test_global_context(void)
{
    PmLogContext context;
    PmLogContext indexed;
    char         name[PMLOG_MAX_CONTEXT_NAME_LEN + 1];

    // kPmLogGlobalContext is the NULL sentinel callers pass in; asking
    // for it by name hands back the pointer the table really holds
    CHECK_EQ(PmLogGetContext(NULL, &context), kPmLogErr_None);
    CHECK(context != NULL);

    CHECK_EQ(PmLogGetContextName(context, name, sizeof(name)),
             kPmLogErr_None);
    CHECK_STR_EQ(name, kPmLogGlobalContextName);

    CHECK_EQ(PmLogGetContextName(kPmLogGlobalContext, name, sizeof(name)),
             kPmLogErr_None);
    CHECK_STR_EQ(name, kPmLogGlobalContextName);

    // the global context is index 0, and the two spellings have to be
    // the same context underneath
    CHECK_EQ(PmLogGetIndContext(0, &indexed), kPmLogErr_None);
    CHECK(indexed == context);

    PmLogLevel level;
    CHECK_EQ(PmLogSetContextLevel(kPmLogGlobalContext, kPmLogLevel_Notice),
             kPmLogErr_None);
    CHECK_EQ(PmLogGetContextLevel(context, &level), kPmLogErr_None);
    CHECK_EQ(level, kPmLogLevel_Notice);
}

static void test_get_is_idempotent(void)
{
    PmLogContext first;
    PmLogContext again;
    int          before;
    int          after;

    CHECK_EQ(PmLogGetNumContexts(&before), kPmLogErr_None);

    CHECK_EQ(PmLogGetContext("test.idempotent", &first), kPmLogErr_None);
    CHECK(first != NULL);

    CHECK_EQ(PmLogGetContext("test.idempotent", &again), kPmLogErr_None);
    CHECK(first == again);

    CHECK_EQ(PmLogGetNumContexts(&after), kPmLogErr_None);
    CHECK_EQ(after, before + 1);
}

static void test_find_does_not_create(void)
{
    PmLogContext context;
    int          before;
    int          after;

    CHECK_EQ(PmLogGetNumContexts(&before), kPmLogErr_None);
    CHECK_EQ(PmLogFindContext("test.never.registered", &context),
             kPmLogErr_ContextNotFound);
    CHECK_EQ(PmLogGetNumContexts(&after), kPmLogErr_None);
    CHECK_EQ(after, before);
}

static void test_levels_round_trip(void)
{
    PmLogContext context;
    PmLogLevel   level;

    CHECK_EQ(PmLogGetContext("test.levels", &context), kPmLogErr_None);

    const PmLogLevel levels[] = {
        kPmLogLevel_Emergency, kPmLogLevel_Alert, kPmLogLevel_Critical,
        kPmLogLevel_Error, kPmLogLevel_Warning, kPmLogLevel_Notice,
        kPmLogLevel_Info, kPmLogLevel_Debug
    };

    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++)
    {
        CHECK_EQ(PmLogSetContextLevel(context, levels[i]), kPmLogErr_None);
        CHECK_EQ(PmLogGetContextLevel(context, &level), kPmLogErr_None);
        CHECK_EQ(level, levels[i]);

        const char* str = PmLogLevelToString(levels[i]);
        CHECK(str != NULL);

        const int* parsed = PmLogStringToLevel(str);
        CHECK(parsed != NULL);
        if (parsed != NULL)
        {
            CHECK_EQ(*parsed, levels[i]);
        }
    }

    CHECK_EQ(PmLogSetContextLevel(context, (PmLogLevel) 42),
             kPmLogErr_InvalidLevel);
}

static void test_name_round_trip(void)
{
    PmLogContext context;
    char         name[PMLOG_MAX_CONTEXT_NAME_LEN + 1];

    CHECK_EQ(PmLogGetContext("test.name.round.trip", &context), kPmLogErr_None);
    CHECK_EQ(PmLogGetContextName(context, name, sizeof(name)), kPmLogErr_None);
    CHECK_STR_EQ(name, "test.name.round.trip");

    // a buffer that cannot hold the name must be refused, not truncated
    char small[4];
    CHECK_EQ(PmLogGetContextName(context, small, sizeof(small)),
             kPmLogErr_BufferTooSmall);
}

static void test_bad_arguments(void)
{
    PmLogContext context;
    char         name[16];

    CHECK_EQ(PmLogGetContext("test.args", NULL), kPmLogErr_InvalidParameter);
    CHECK_EQ(PmLogGetNumContexts(NULL), kPmLogErr_InvalidParameter);
    CHECK_EQ(PmLogGetIndContext(0, NULL), kPmLogErr_InvalidParameter);
    CHECK_EQ(PmLogGetIndContext(-1, &context), kPmLogErr_InvalidContextIndex);
    CHECK_EQ(PmLogGetContextName(kPmLogGlobalContext, NULL, sizeof(name)),
             kPmLogErr_InvalidParameter);
    CHECK_EQ(PmLogGetContextLevel(kPmLogGlobalContext, NULL),
             kPmLogErr_InvalidParameter);

    // a name that cannot fit in the table is rejected
    char tooLong[PMLOG_MAX_CONTEXT_NAME_LEN + 32];
    memset(tooLong, 'x', sizeof(tooLong) - 1);
    tooLong[sizeof(tooLong) - 1] = 0;
    CHECK_EQ(PmLogGetContext(tooLong, &context), kPmLogErr_InvalidContextName);

    CHECK(PmLogGetErrDbgString(kPmLogErr_None) != NULL);
    CHECK(PmLogGetErrDbgString(kPmLogErr_InvalidParameter) != NULL);
}

static void test_indexed_walk_matches_names(void)
{
    int numContexts;
    CHECK_EQ(PmLogGetNumContexts(&numContexts), kPmLogErr_None);
    CHECK(numContexts >= 1);

    for (int i = 0; i < numContexts; i++)
    {
        PmLogContext context;
        char         name[PMLOG_MAX_CONTEXT_NAME_LEN + 1];

        CHECK_EQ(PmLogGetIndContext(i, &context), kPmLogErr_None);
        CHECK_EQ(PmLogGetContextName(context, name, sizeof(name)),
                 kPmLogErr_None);
        CHECK(name[0] != 0);

        // every name in the table must resolve back to the same context
        PmLogContext found;
        CHECK_EQ(PmLogFindContext(name, &found), kPmLogErr_None);
        CHECK(found == context);
    }

    PmLogContext context;
    CHECK_EQ(PmLogGetIndContext(numContexts, &context),
             kPmLogErr_InvalidContextIndex);
}

int main(void)
{
    test_global_context();
    test_get_is_idempotent();
    test_find_does_not_create();
    test_levels_round_trip();
    test_name_round_trip();
    test_bad_arguments();
    test_indexed_walk_matches_names();

    return test_report("test_contexts");
}
