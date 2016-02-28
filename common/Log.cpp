/*
 * Copyright (c) 2016 Philip Taylor
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "common/Common.h"

#include "common/Log.h"

#include <cstdarg>
#include <cstdio>

void PrintMessage(const char *msg)
{
    printf("%s", msg);
#ifdef _WIN32
    // It's awkward to read stdout in Visual Studio, so duplicate the message
    // into VS's debug output window
    OutputDebugStringA(msg);
#endif
}

void PrintfMessage(const char *fmt, ...)
{
    const size_t MAX_MESSAGE_SIZE = 1024;
    char buf[MAX_MESSAGE_SIZE];

    va_list ap;
    va_start(ap, fmt);
#ifdef _WIN32
    _vsnprintf(buf, MAX_MESSAGE_SIZE, fmt, ap);
#else
    vsnprintf(buf, MAX_MESSAGE_SIZE, fmt, ap);
#endif
    buf[MAX_MESSAGE_SIZE - 1] = '\0';
    va_end(ap);
    PrintMessage(buf);
}
