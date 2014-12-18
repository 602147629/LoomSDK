/*
 * ===========================================================================
 * Loom SDK
 * Copyright 2011, 2012, 2013
 * The Game Engine Company, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ===========================================================================
 */

#include "lsLog.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "loom/script/loomscript.h"

namespace LS {
static FunctionLog externLog = 0;

static void *externExtra = 0;

// mappings to external logging
static int externLogInfo  = -1;
static int externLogWarn  = -1;
static int externLogError = -1;

static LSLogLevel logLevel = LSLogInfo;

void LSLogInitialize(FunctionLog log, void *extra, int logInfo, int logWarn,
                     int logError)
{
    externExtra    = extra;
    externLog      = log;
    externLogInfo  = logInfo;
    externLogWarn  = logWarn;
    externLogError = logError;
}


void LSLogSetLevel(LSLogLevel level)
{
    logLevel = level;
}

#ifndef _MSC_VER
int _vscprintf(const char * format, va_list pargs)
{
    int retval;
    va_list argcopy;
    va_copy(argcopy, pargs);
    retval = vsnprintf(NULL, 0, format, argcopy);
    va_end(argcopy);
    return retval;
}
#endif

void LSLog(LSLogLevel level, const char *format, ...)
{
    if (level < logLevel)
    {
        return;
    }

    char* buff = loom_log_getArgs(&format);

    /*
    va_list args;
    va_start(args, format);
    int count = _vscprintf(format, args);
    char* buff = (char*)malloc(count+2);
    vsprintf_s(buff, count+1, format, args);
    va_end(args);
    */

    /*
    char    buff[2048];
    #ifdef _MSC_VER
    vsprintf_s(buff, 2046, format, args);
    #else
    vsnprintf(buff, 2046, format, args);
    #endif
    va_end(args); va_start(args, format);
    //*/

    if (externLog)
    {
        int elevel;

        switch (level)
        {
        case LSLogQuiet:
            elevel = externLogInfo;
            break;

        case LSLogInfo:
            elevel = externLogInfo;
            break;

        case LSLogWarn:
            elevel = externLogWarn;
            break;

        case LSLogError:
            elevel = externLogError;
            break;
        }

        externLog(externExtra, elevel, "%s", buff);
    } else {
        printf("%s\n", buff);
    }

    free(buff);

}
}
