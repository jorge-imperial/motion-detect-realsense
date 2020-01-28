
#include <iostream>
#include <librealsense2/rs.hpp>



int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

size_t convert_points(rs2::points& points, std::vector<rs2::vertex> & cloudp) {

    auto vertices = points.get_vertices();              // get vertices

    cloudp.clear();
    for (size_t i = 0; i < points.size(); i++) {
        if (vertices[i].z != 0.0f)  {
            cloudp.push_back(vertices[i]);

            // std::cout << vertices[i].x << "," << vertices[i].y << "," << vertices[i].z << "," << std::endl;
        }
    }

    return points.size();
}


 

template<typename ... Args> std::string string_format( const std::string& format, Args ... args )
{
    size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf( new char[ size ] );
    snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}
