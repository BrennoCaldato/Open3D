// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
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

#include <string>
#include <vector>

#include "open3d/pipelines/registration/PoseGraph.h"
#include "open3d/t/pipelines/slac/ControlGrid.h"
#include "open3d/t/pipelines/slac/Visualization.h"

namespace open3d {
namespace t {
namespace pipelines {
namespace slac {

using PoseGraph = open3d::pipelines::registration::PoseGraph;

struct SLACOptimizerOption {
    int max_iterations_ = 10;

    float voxel_size_ = 0.05;
    float regularizor_coeff_ = 1;
    float threshold_ = 0.07;

    bool debug_ = false;
    int debug_start_idx_ = 0;
    int debug_start_itr_ = 1;

    bool debug_enabled_ = false;

    std::string device_ = "CPU:0";

    std::string buffer_folder_ = "";
    std::string GetSubfolderName() const {
        if (voxel_size_ < 0) {
            return fmt::format("{}/original", buffer_folder_);
        }
        return fmt::format("{}/{:.3f}", buffer_folder_, voxel_size_);
    }
};

/// \brief Read pose graph containing loop closures and odometry to compute
/// correspondences. Uses aggressive pruning -- reject any suspicious pair.
///
/// \param fnames_processed Vector of filenames for processed pointcloud
/// fragments. \param fragment_pose_graph Legacy PoseGraph for pointcloud
/// fragments. \param option SLACOptimizerOption containing the configurations.
void SaveCorrespondencesForPointClouds(
        const std::vector<std::string>& fnames_processed,
        const PoseGraph& fragment_pose_graph,
        const SLACOptimizerOption& option);

/// \brief Simultaneous Localization and Calibration: Self-Calibration of
/// Consumer Depth Cameras, CVPR 2014 Qian-Yi Zhou and Vladlen Koltun Estimate a
/// shared control grid for all fragments for scene reconstruction, implemented
/// in https://github.com/qianyizh/ElasticReconstruction.
///
/// \param fragment_fnames Vector of filenames for pointcloud fragments.
/// \param fragment_pose_graph Legacy PoseGraph for pointcloud fragments.
/// \param option SLACOptimizerOption containing the configurations.
/// \return pair of registraion::PoseGraph and slac::ControlGrid.
std::pair<PoseGraph, ControlGrid> RunSLACOptimizerForFragments(
        const std::vector<std::string>& fragment_fnames,
        const PoseGraph& fragment_pose_graph,
        SLACOptimizerOption& option);

PoseGraph RunRigidOptimizerForFragments(
        const std::vector<std::string>& fragment_fnames,
        const PoseGraph& fragment_pose_graph,
        SLACOptimizerOption& option);

}  // namespace slac
}  // namespace pipelines
}  // namespace t
}  // namespace open3d