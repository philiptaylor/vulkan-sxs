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

#ifndef INCLUDED_VKSXS_LOG
#define INCLUDED_VKSXS_LOG

void PrintMessage(const char *msg);
void PrintfMessage(const char *fmt, ...);

// Trivial logging system
#define LOGI(fmt, ...) do { PrintfMessage("[INFO] " fmt "\n", __VA_ARGS__); } while (0)
#define LOGW(fmt, ...) do { PrintfMessage("[WARN] " fmt "\n", __VA_ARGS__); } while (0)
#define LOGE(fmt, ...) do { PrintfMessage("[ERROR] " fmt "\n", __VA_ARGS__); } while (0)

#define ASSERT(cond) do { if (!(cond)) { LOGE("Assertion failed: %s:%d: %s", __FILE__, __LINE__, #cond); abort(); } } while (0)

#endif // INCLUDED_VKSXS_LOG
