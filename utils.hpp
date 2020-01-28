#pragma once

#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>
#include <vector>

int64_t now_ms();
size_t convert_points(rs2::points& points, std::vector<rs2::vertex> & cloudp);

 
