/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/os_interface/print.h"

#include <iostream>
#include <cstdio>

void printToSTDOUT(const char *str) {
    fprintf(stdout, "%s", str);
    fflush(stdout);
}

template <class T>
size_t simple_sprintf(char *output, size_t outputSize, const char *format, T value) {
    return snprintf(output, outputSize, format, value);
}

size_t simple_sprintf(char *output, size_t outputSize, const char *format, const char *value) {
    return snprintf(output, outputSize, format, value);
}

size_t simple_sprintf(char *output, size_t outputSize, const char *format, void *value) {
    return snprintf(output, outputSize, format, value);
}

template size_t simple_sprintf<float>(char *output, size_t output_size, const char *format, float value);
template size_t simple_sprintf<double>(char *output, size_t output_size, const char *format, double value);
template size_t simple_sprintf<char>(char *output, size_t output_size, const char *format, char value);
template size_t simple_sprintf<int8_t>(char *output, size_t output_size, const char *format, int8_t value);
template size_t simple_sprintf<int16_t>(char *output, size_t output_size, const char *format, int16_t value);
template size_t simple_sprintf<int32_t>(char *output, size_t output_size, const char *format, int32_t value);
template size_t simple_sprintf<int64_t>(char *output, size_t output_size, const char *format, int64_t value);
template size_t simple_sprintf<uint8_t>(char *output, size_t output_size, const char *format, uint8_t value);
template size_t simple_sprintf<uint16_t>(char *output, size_t output_size, const char *format, uint16_t value);
template size_t simple_sprintf<uint32_t>(char *output, size_t output_size, const char *format, uint32_t value);
template size_t simple_sprintf<uint64_t>(char *output, size_t output_size, const char *format, uint64_t value);
