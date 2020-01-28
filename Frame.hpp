
#pragma once

 
#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>
#include <vector>
#include <queue>

class Frame {
    private:
        std::vector<rs2::vertex> vert;
        uint32_t frame_number;
        uint64_t timestamp;

    public:
        Frame() {};
        Frame(std::vector<rs2::vertex> v, int32_t f, uint64_t t) {
            vert = v;
            frame_number = f;
            timestamp = t;
        }
        std::vector<rs2::vertex> get_vertices() { return vert; }
        uint32_t get_frame_number() { return frame_number; }
};