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
* @brief The context table is shared between processes.
*
* Each child re-execs this binary so the library constructor runs again
* and has to attach to the block the parent already created - a plain
* fork() would inherit the parent's mapping and prove nothing.
*
* @file test_sharing.c
**/

#include "test_util.h"

#include <PmLogLib.h>

#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>

// the level constants are deliberately deprecated in the public header
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define NUM_CHILDREN 4

// argv[0] of this binary, so a child can re-exec it
static const char* g_self;

/**
@brief  Child mode: register the context named by argv[2] at the level
        given in argv[3], then exit.
**/
static int child_register(const char* name, int level)
{
    PmLogContext context;

    if (PmLogGetContext(name, &context) != kPmLogErr_None)
    {
        fprintf(stderr, "child: PmLogGetContext(%s) failed\n", name);
        return EXIT_FAILURE;
    }

    if (PmLogSetContextLevel(context, (PmLogLevel) level) != kPmLogErr_None)
    {
        fprintf(stderr, "child: PmLogSetContextLevel(%s) failed\n", name);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**
@brief  Child mode: the context named by argv[2] must already be in the
        table, at the level given in argv[3].
**/
static int child_expect(const char* name, int level)
{
    PmLogContext context;
    PmLogLevel   actual;

    if (PmLogFindContext(name, &context) != kPmLogErr_None)
    {
        fprintf(stderr, "child: %s is not in the shared table\n", name);
        return EXIT_FAILURE;
    }

    if (PmLogGetContextLevel(context, &actual) != kPmLogErr_None)
    {
        fprintf(stderr, "child: cannot read the level of %s\n", name);
        return EXIT_FAILURE;
    }

    if ((int) actual != level)
    {
        fprintf(stderr, "child: %s is at level %d, expected %d\n",
                name, (int) actual, level);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static bool run_child(const char* mode, const char* name, int level)
{
    char levelStr[16];
    snprintf(levelStr, sizeof(levelStr), "%d", level);

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return false;
    }

    if (pid == 0)
    {
        execl(g_self, g_self, mode, name, levelStr, (char*) NULL);
        perror("execl");
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) != pid)
    {
        perror("waitpid");
        return false;
    }

    return WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_SUCCESS);
}

int main(int argc, char** argv)
{
    g_self = argv[0];

    if (argc == 4)
    {
        // child mode
        int level = atoi(argv[3]);

        if (strcmp(argv[1], "register") == 0)
        {
            return child_register(argv[2], level);
        }

        if (strcmp(argv[1], "expect") == 0)
        {
            return child_expect(argv[2], level);
        }

        fprintf(stderr, "unknown child mode %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    int before;
    CHECK_EQ(PmLogGetNumContexts(&before), kPmLogErr_None);

    // what the children register must show up here
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        char name[64];
        snprintf(name, sizeof(name), "test.sharing.child.%d", i);

        CHECK(run_child("register", name, kPmLogLevel_Warning));

        PmLogContext context;
        PmLogLevel   level;
        CHECK_EQ(PmLogFindContext(name, &context), kPmLogErr_None);
        CHECK_EQ(PmLogGetContextLevel(context, &level), kPmLogErr_None);
        CHECK_EQ(level, kPmLogLevel_Warning);
    }

    int after;
    CHECK_EQ(PmLogGetNumContexts(&after), kPmLogErr_None);
    CHECK_EQ(after, before + NUM_CHILDREN);

    // and what we register must show up in a freshly started process
    PmLogContext parentContext;
    CHECK_EQ(PmLogGetContext("test.sharing.parent", &parentContext),
             kPmLogErr_None);
    CHECK_EQ(PmLogSetContextLevel(parentContext, kPmLogLevel_Critical),
             kPmLogErr_None);
    CHECK(run_child("expect", "test.sharing.parent", kPmLogLevel_Critical));

    // a level changed by a child must be visible here straight away
    CHECK(run_child("register", "test.sharing.parent", kPmLogLevel_Debug));
    PmLogLevel level;
    CHECK_EQ(PmLogGetContextLevel(parentContext, &level), kPmLogErr_None);
    CHECK_EQ(level, kPmLogLevel_Debug);

    return test_report("test_sharing");
}
