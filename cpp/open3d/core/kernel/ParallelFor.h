// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018-2021 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <functional>

namespace open3d {
namespace core {
namespace kernel {

// 1. Is the name too general? I.e. it only applies for small jobs.
// 2. Do we need a wrapper, i.e. shall we simply use
//    `#pragma omp parallel for if (xxx)`?

/// Parallel for loop with default minimal_chunk size.
///
/// \param num_jobs Number of jobs. \p f will be called from 0 to \p num_jobs
/// - 1. That is `f(0)`, `f(1)`, ..., `f(num_jobs - 1)`.
/// \param f Function to be executed in parallel. The function shall have the
/// signature `void f(int64_t)`. The function shall be embarrassingly
/// parallelizable.
void ParallelFor(int64_t num_jobs, const std::function<void(int64_t)>& f);

/// Parallel for loop with default minimal_chunk size.
///
/// \param start The start index, inclusive.
/// \param end The end index, exclusive.
/// \param f Function to be executed in parallel. The function shall have the
/// signature `void f(int64_t)`. The function shall be embarrassingly
/// parallelizable.
void ParallelFor(int64_t start,
                 int64_t end,
                 const std::function<void(int64_t)>& f);

/// Parallel for loop.
///
/// \param start The start index, inclusive.
/// \param end The end index, exclusive.
/// \param min_parallel_size If end - start <= min_parallel_size, the job will
/// be executed in serial.
/// \param f Function to be executed in parallel. The function shall have the
/// signature `void f(int64_t)`. The function shall be embarrassingly
/// parallelizable.
void ParallelFor(int64_t start,
                 int64_t end,
                 int64_t min_parallel_size,
                 const std::function<void(int64_t)>& f);

}  // namespace kernel
}  // namespace core
}  // namespace open3d
