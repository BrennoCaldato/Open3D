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

#include "open3d/core/Dispatch.h"
#include "open3d/core/Dtype.h"
#include "open3d/core/MemoryManager.h"
#include "open3d/core/SizeVector.h"
#include "open3d/core/Tensor.h"
#include "open3d/core/kernel/CPULauncher.h"
#include "open3d/core/nns/NearestNeighborSearch.h"
#include "open3d/t/geometry/kernel/GeometryIndexer.h"
#include "open3d/t/geometry/kernel/GeometryMacros.h"
#include "open3d/t/geometry/kernel/PointCloud.h"
#include "open3d/t/geometry/kernel/PointCloudImpl.h"
#include "open3d/t/pipelines/kernel/SVD3x3CPU.h"
#include "open3d/utility/Console.h"
#include "open3d/utility/Eigen.h"

namespace open3d {
namespace t {
namespace geometry {
namespace kernel {
namespace pointcloud {

void ProjectCPU(
        core::Tensor& depth,
        utility::optional<std::reference_wrapper<core::Tensor>> image_colors,
        const core::Tensor& points,
        utility::optional<std::reference_wrapper<const core::Tensor>> colors,
        const core::Tensor& intrinsics,
        const core::Tensor& extrinsics,
        float depth_scale,
        float depth_max) {
    const bool has_colors = image_colors.has_value();

    int64_t n = points.GetLength();

    const float* points_ptr = points.GetDataPtr<float>();
    const float* point_colors_ptr =
            has_colors ? colors.value().get().GetDataPtr<float>() : nullptr;

    TransformIndexer transform_indexer(intrinsics, extrinsics, 1.0f);
    NDArrayIndexer depth_indexer(depth, 2);

    NDArrayIndexer color_indexer;
    if (has_colors) {
        color_indexer = NDArrayIndexer(image_colors.value().get(), 2);
    }

    core::kernel::CPULauncher::LaunchGeneralKernel(
            n, [&](int64_t workload_idx) {
                float x = points_ptr[3 * workload_idx + 0];
                float y = points_ptr[3 * workload_idx + 1];
                float z = points_ptr[3 * workload_idx + 2];

                // coordinate in camera (in voxel -> in meter)
                float xc, yc, zc, u, v;
                transform_indexer.RigidTransform(x, y, z, &xc, &yc, &zc);

                // coordinate in image (in pixel)
                transform_indexer.Project(xc, yc, zc, &u, &v);
                if (!depth_indexer.InBoundary(u, v) || zc <= 0 ||
                    zc > depth_max) {
                    return;
                }

                float* depth_ptr = depth_indexer.GetDataPtr<float>(
                        static_cast<int64_t>(u), static_cast<int64_t>(v));
                float d = zc * depth_scale;
#pragma omp critical
                {
                    if (*depth_ptr == 0 || *depth_ptr >= d) {
                        *depth_ptr = d;

                        if (has_colors) {
                            uint8_t* color_ptr =
                                    color_indexer.GetDataPtr<uint8_t>(
                                            static_cast<int64_t>(u),
                                            static_cast<int64_t>(v));

                            color_ptr[0] = static_cast<uint8_t>(
                                    point_colors_ptr[3 * workload_idx + 0] *
                                    255.0);
                            color_ptr[1] = static_cast<uint8_t>(
                                    point_colors_ptr[3 * workload_idx + 1] *
                                    255.0);
                            color_ptr[2] = static_cast<uint8_t>(
                                    point_colors_ptr[3 * workload_idx + 2] *
                                    255.0);
                        }
                    }
                }
            });
}

void EstimatePointWiseColorGradientCPU(const core::Tensor& points,
                                       const core::Tensor& normals,
                                       const core::Tensor& colors,
                                       core::Tensor& color_gradients,
                                       const double& radius,
                                       const int64_t& max_nn) {
    int64_t n = points.GetLength();

    core::nns::NearestNeighborSearch tree(points);

    bool check = tree.HybridIndex(radius);
    if (!check) {
        utility::LogError(
                "NearestNeighborSearch::FixedRadiusIndex Index is not set.");
    }

    core::Tensor indices, distance, counts;
    std::tie(indices, distance, counts) =
            tree.HybridSearch(points, radius, max_nn);

    const float* points_ptr = points.GetDataPtr<float>();
    const float* normals_ptr = normals.GetDataPtr<float>();
    const float* colors_ptr = colors.GetDataPtr<float>();
    const int64_t* neighbour_indices_ptr = indices.GetDataPtr<int64_t>();
    const int64_t* neighbour_counts_ptr = counts.GetDataPtr<int64_t>();

    float* color_gradients_ptr = color_gradients.GetDataPtr<float>();

#pragma omp parallel for schedule(static)
    for (int64_t workload_idx = 0; workload_idx < n; workload_idx++) {
        // NNS.
        int64_t neighbour_offset = max_nn * workload_idx;
        int64_t neighbour_count = neighbour_counts_ptr[workload_idx];
        int64_t point_idx = 3 * workload_idx;

        if (neighbour_count >= 4) {
            float vt[3] = {points_ptr[point_idx], points_ptr[point_idx + 1],
                           points_ptr[point_idx + 2]};

            float nt[3] = {normals_ptr[point_idx], normals_ptr[point_idx + 1],
                           normals_ptr[point_idx + 2]};

            float it = (colors_ptr[point_idx] + colors_ptr[point_idx + 1] +
                        colors_ptr[point_idx + 2]) /
                       3.0;

            float AtA[9] = {0};
            float Atb[3] = {0};

            // approximate image gradient of vt's tangential plane
            // projection (p') of a point p on a plane defined by normal n,
            // where o is the closest point to p on the plane, is given by:
            // p' = p - [(p - o).dot(n)] * n
            // p' = p - [(p.dot(n) - s)] * n [where s = o.dot(n)]
            // Computing the scalar s.
            float s = vt[0] * nt[0] + vt[1] * nt[1] + vt[2] * nt[2];

            int i = 1;
            for (i = 1; i < neighbour_count; i++) {
                int64_t neighbour_idx =
                        3 * neighbour_indices_ptr[neighbour_offset + i];

                if (neighbour_idx == -1) {
                    break;
                }

                float vt_adj[3] = {points_ptr[neighbour_idx],
                                   points_ptr[neighbour_idx + 1],
                                   points_ptr[neighbour_idx + 2]};

                // p' = p - d * n [where d = p.dot(n) - s]
                // Computing the scalar d.
                float d = vt_adj[0] * nt[0] + vt_adj[1] * nt[1] +
                          vt_adj[2] * nt[2] - s;

                // Computing the p' (projection of the point).
                float vt_proj[3] = {vt_adj[0] - d * nt[0],
                                    vt_adj[1] - d * nt[1],
                                    vt_adj[2] - d * nt[2]};

                float it_adj = (colors_ptr[neighbour_idx + 0] +
                                colors_ptr[neighbour_idx + 1] +
                                colors_ptr[neighbour_idx + 2]) /
                               3.0;

                float A[3] = {vt_proj[0] - vt[0], vt_proj[1] - vt[1],
                              vt_proj[2] - vt[2]};

                AtA[0] += A[0] * A[0];
                AtA[1] += A[1] * A[0];
                AtA[2] += A[2] * A[0];
                AtA[4] += A[1] * A[1];
                AtA[5] += A[2] * A[1];
                AtA[8] += A[2] * A[2];

                float b = it_adj - it;

                Atb[0] += A[0] * b;
                Atb[1] += A[1] * b;
                Atb[2] += A[2] * b;
            }

            // Orthogonal constraint.
            float A[3] = {(i - 1) * nt[0], (i - 1) * nt[1], (i - 1) * nt[2]};

            AtA[0] += A[0] * A[0];
            AtA[1] += A[0] * A[1];
            AtA[2] += A[0] * A[2];
            AtA[4] += A[1] * A[1];
            AtA[5] += A[1] * A[2];
            AtA[8] += A[2] * A[2];

            // Symmetry.
            AtA[3] = AtA[1];
            AtA[6] = AtA[2];
            AtA[7] = AtA[5];

            solve_svd3x3(AtA[0], AtA[1], AtA[2], AtA[3], AtA[4], AtA[5], AtA[6],
                         AtA[7], AtA[8], Atb[0], Atb[1], Atb[2],
                         color_gradients_ptr[point_idx + 0],
                         color_gradients_ptr[point_idx + 1],
                         color_gradients_ptr[point_idx + 2]);

            // DEBUG: To compare with Open3D core::Tensor::Solve()
            // core::Tensor ata = core::Tensor::Init<float>(
            //         {{AtA[0], AtA[1], AtA[2]},
            //          {AtA[3], AtA[4], AtA[5]},
            //          {AtA[6], AtA[7], AtA[8]}});
            // core::Tensor atb = core::Tensor::Init<float>(
            //         {{Atb[0]}, {Atb[1]}, {Atb[2]}});
            // core::Tensor x = ata.To(core::Dtype::Float64)
            //                          .Solve(atb.To(core::Dtype::Float64))
            //                          .To(core::Dtype::Float32);
            // auto x_ptr = x.GetDataPtr<float>();
            // color_gradients_ptr[point_idx] = x_ptr[0];
            // color_gradients_ptr[point_idx + 1] = x_ptr[1];
            // color_gradients_ptr[point_idx + 2] = x_ptr[2];
            // printf("\n X: %f, %f, %f", x_ptr[0], x_ptr[1], x_ptr[2]);
        } else {
            color_gradients_ptr[point_idx] = 0;
            color_gradients_ptr[point_idx + 1] = 0;
            color_gradients_ptr[point_idx + 2] = 0;
        }
    }
}

void EstimatePointWiseCovarianceCPU(const core::Tensor& points,
                                    core::Tensor& covariances,
                                    const double& radius,
                                    const int64_t& max_nn) {
    int64_t n = points.GetLength();

    core::nns::NearestNeighborSearch tree(points);

    bool check = tree.HybridIndex(radius);
    if (!check) {
        utility::LogError(
                "NearestNeighborSearch::FixedRadiusIndex Index is not set.");
    }

    core::Tensor indices, distance, counts;
    std::tie(indices, distance, counts) =
            tree.HybridSearch(points, radius, max_nn);

    const float* points_ptr = points.GetDataPtr<float>();
    const int64_t* neighbour_indices_ptr = indices.GetDataPtr<int64_t>();
    const int64_t* neighbour_counts_ptr = counts.GetDataPtr<int64_t>();

    float* covariances_ptr = covariances.GetDataPtr<float>();

#pragma omp parallel for schedule(static)
    for (int64_t workload_idx = 0; workload_idx < n; workload_idx++) {
        // NNS.
        int64_t neighbour_offset = max_nn * workload_idx;
        int64_t neighbour_count = neighbour_counts_ptr[workload_idx];
        // int64_t point_idx = 3 * workload_idx;
        int64_t covariances_offset = 9 * workload_idx;

        if (neighbour_count >= 3) {
            EstimatePointWiseCovarianceKernel(
                    points_ptr, neighbour_indices_ptr, neighbour_count,
                    covariances_ptr, neighbour_offset, covariances_offset);
        } else {
            // Identity.
            covariances_ptr[covariances_offset] = 1.0;
            covariances_ptr[covariances_offset + 1] = 0.0;
            covariances_ptr[covariances_offset + 2] = 0.0;
            covariances_ptr[covariances_offset + 3] = 0.0;
            covariances_ptr[covariances_offset + 4] = 1.0;
            covariances_ptr[covariances_offset + 5] = 0.0;
            covariances_ptr[covariances_offset + 6] = 0.0;
            covariances_ptr[covariances_offset + 7] = 0.0;
            covariances_ptr[covariances_offset + 8] = 1.0;
        }
    }
}

}  // namespace pointcloud
}  // namespace kernel
}  // namespace geometry
}  // namespace t
}  // namespace open3d
