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
* @brief Hammer the shared context table from several threads at once,
*        then check it is still consistent.
*
* The table lives in shared memory and is guarded by a file lock plus a
* process local mutex; this is the test that says whether that holds up.
* Run it under ThreadSanitizer to check the locking itself, and with
* --processes to have several copies race each other through the file
* lock.
*
* @file test_stress.c
**/

#include "test_util.h"

#include <PmLogLib.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// the level constants are deliberately deprecated in the public header
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define MAX_THREADS 64

typedef struct
{
    int      id;
    int      iterations;
    int      contextsPerThread;
    unsigned tag;
    long     operations;
} ThreadArgs;

static const PmLogLevel kLevels[] = {
    kPmLogLevel_Error, kPmLogLevel_Warning, kPmLogLevel_Notice,
    kPmLogLevel_Info, kPmLogLevel_Debug
};

static void context_name(char* buf, size_t size, unsigned tag, int thread,
    int index)
{
    snprintf(buf, size, "stress.%u.%d.%d", tag, thread, index);
}

static uint32_t next_random(uint32_t* state)
{
    // xorshift32: no global state, so threads don't serialize on rand()
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void* worker(void* argP)
{
    ThreadArgs* args = (ThreadArgs*) argP;
    uint32_t    random = (uint32_t) (args->id + 1) * 2654435761u;
    char        name[64];

    for (int i = 0; i < args->iterations; i++)
    {
        int index = (int) (next_random(&random) % (uint32_t) args->contextsPerThread);
        context_name(name, sizeof(name), args->tag, args->id, index);

        PmLogContext context;
        if (PmLogGetContext(name, &context) != kPmLogErr_None)
        {
            fprintf(stderr, "thread %d: PmLogGetContext(%s) failed\n",
                    args->id, name);
            continue;
        }

        PmLogLevel level = kLevels[next_random(&random) % (sizeof(kLevels) / sizeof(kLevels[0]))];
        (void) PmLogSetContextLevel(context, level);

        PmLogLevel readBack;
        (void) PmLogGetContextLevel(context, &readBack);

        // read a context belonging to a different thread
        int other = (int) (next_random(&random) % (uint32_t) MAX_THREADS);
        context_name(name, sizeof(name), args->tag, other, index);
        PmLogContext otherContext;
        (void) PmLogFindContext(name, &otherContext);

        // the level check and format validation on a line that is
        // filtered out: the path every log call takes, without filling
        // up syslog
        (void) PmLogSetContextLevel(context, kPmLogLevel_Emergency);
        (void) PmLogString_(context, kPmLogLevel_Debug, "STRESS",
                            "{\"thread\":\"stress\"}", "message");

        args->operations += 5;
    }

    return NULL;
}

/**
@brief  Every context in the table must have a name, that name must
        resolve back to the same context, and no two entries may share
        a name.
**/
static void check_table_integrity(void)
{
    int numContexts;
    CHECK_EQ(PmLogGetNumContexts(&numContexts), kPmLogErr_None);
    CHECK(numContexts > 0);

    if (numContexts <= 0)
    {
        return;
    }

    char (*names)[PMLOG_MAX_CONTEXT_NAME_LEN + 1] =
        calloc((size_t) numContexts, PMLOG_MAX_CONTEXT_NAME_LEN + 1);
    CHECK(names != NULL);
    if (names == NULL)
    {
        return;
    }

    for (int i = 0; i < numContexts; i++)
    {
        PmLogContext context;
        CHECK_EQ(PmLogGetIndContext(i, &context), kPmLogErr_None);
        CHECK_EQ(PmLogGetContextName(context, names[i],
                                     PMLOG_MAX_CONTEXT_NAME_LEN + 1),
                 kPmLogErr_None);
        CHECK(names[i][0] != 0);

        PmLogContext found;
        CHECK_EQ(PmLogFindContext(names[i], &found), kPmLogErr_None);
        CHECK(found == context);
    }

    for (int i = 0; i < numContexts; i++)
    {
        for (int j = i + 1; j < numContexts; j++)
        {
            if (strcmp(names[i], names[j]) == 0)
            {
                g_failures++;
                fprintf(stderr, "FAIL duplicate context %s at %d and %d\n",
                        names[i], i, j);
            }
            g_checks++;
        }
    }

    free(names);
}

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

static void usage(const char* self)
{
    fprintf(stderr,
            "usage: %s [--threads N] [--iterations N] [--contexts N] "
            "[--processes N] [--tag N]\n", self);
}

int main(int argc, char** argv)
{
    int threads = 8;
    int iterations = 500;
    int contextsPerThread = 8;
    int processes = 1;
    unsigned tag = (unsigned) getpid();

    for (int i = 1; i < argc; i++)
    {
        if ((i + 1 < argc) && (strcmp(argv[i], "--threads") == 0))
        {
            threads = atoi(argv[++i]);
        }
        else if ((i + 1 < argc) && (strcmp(argv[i], "--iterations") == 0))
        {
            iterations = atoi(argv[++i]);
        }
        else if ((i + 1 < argc) && (strcmp(argv[i], "--contexts") == 0))
        {
            contextsPerThread = atoi(argv[++i]);
        }
        else if ((i + 1 < argc) && (strcmp(argv[i], "--processes") == 0))
        {
            processes = atoi(argv[++i]);
        }
        else if ((i + 1 < argc) && (strcmp(argv[i], "--tag") == 0))
        {
            tag = (unsigned) strtoul(argv[++i], NULL, 10);
        }
        else
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if ((threads < 1) || (threads > MAX_THREADS) || (iterations < 1) ||
        (contextsPerThread < 1) || (processes < 1))
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // the table is a fixed size shared resource: stay inside it
    if (threads * contextsPerThread * processes >= PMLOG_MAX_NUM_CONTEXTS)
    {
        fprintf(stderr,
                "threads x contexts x processes must stay under %d\n",
                PMLOG_MAX_NUM_CONTEXTS);
        return EXIT_FAILURE;
    }

    // extra processes race this one through the file lock; each uses its
    // own tag so the names never collide
    for (int p = 1; p < processes; p++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            break;
        }

        if (pid == 0)
        {
            char threadsStr[16], iterationsStr[16], contextsStr[16], tagStr[16];
            snprintf(threadsStr, sizeof(threadsStr), "%d", threads);
            snprintf(iterationsStr, sizeof(iterationsStr), "%d", iterations);
            snprintf(contextsStr, sizeof(contextsStr), "%d", contextsPerThread);
            snprintf(tagStr, sizeof(tagStr), "%u", tag + (unsigned) p);
            execl(argv[0], argv[0], "--threads", threadsStr,
                  "--iterations", iterationsStr, "--contexts", contextsStr,
                  "--tag", tagStr, (char*) NULL);
            perror("execl");
            _exit(127);
        }
    }

    pthread_t  ids[MAX_THREADS];
    ThreadArgs args[MAX_THREADS];
    double     start = now_seconds();

    for (int i = 0; i < threads; i++)
    {
        args[i] = (ThreadArgs) {
            .id = i,
            .iterations = iterations,
            .contextsPerThread = contextsPerThread,
            .tag = tag,
            .operations = 0
        };

        if (pthread_create(&ids[i], NULL, worker, &args[i]) != 0)
        {
            perror("pthread_create");
            return EXIT_FAILURE;
        }
    }

    long operations = 0;
    for (int i = 0; i < threads; i++)
    {
        (void) pthread_join(ids[i], NULL);
        operations += args[i].operations;
    }

    double elapsed = now_seconds() - start;

    // every context each thread was supposed to create must exist
    for (int t = 0; t < threads; t++)
    {
        for (int c = 0; c < contextsPerThread; c++)
        {
            char         name[64];
            PmLogContext context;
            context_name(name, sizeof(name), tag, t, c);
            CHECK_EQ(PmLogFindContext(name, &context), kPmLogErr_None);
        }
    }

    check_table_integrity();

    int children = 0;
    int status = 0;
    while (wait(&status) > 0)
    {
        children++;
        CHECK(WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_SUCCESS));
    }
    CHECK_EQ(children, processes - 1);

    printf("test_stress: %d threads x %d iterations, %ld operations in %.2fs "
           "(%.0f ops/s)\n", threads, iterations, operations, elapsed,
           elapsed > 0 ? (double) operations / elapsed : 0.0);

    return test_report("test_stress");
}
