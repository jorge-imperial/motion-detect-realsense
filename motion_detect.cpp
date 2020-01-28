// Feed Mongo!
#include <iostream>
#include <vector>
#include <string>

 
#include <signal.h>
static volatile sig_atomic_t sig_caught = 0;


#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/program_options.hpp>
#include "utils.hpp"
#include "MongoStream.hpp"
 

namespace logging = boost::log;
namespace po = boost::program_options;



void handle_sigint(int signum)
{
    // Handling CTRL-C
    if (signum == SIGINT) {
        sig_caught = 1;
    }
}

void init_logging() {
    logging::core::get()->set_filter
    (
        logging::trivial::severity >= logging::trivial::info
    );
}


int main(int argc, char *argv[]) try {

    init_logging();
 
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
    
    // database
        ("mongo-uri,u", po::value<std::string>()->default_value("mongodb://localhost::27017"), "set MongoDB URI.")
        ("mongo-db,d", po::value< std::string >()->default_value("acq"), "set database name.")
        ("mongo-collection,c", po::value< std::string>()->default_value("cam1"), "set collection name.")
        ("capped", po::value<std::string>()->default_value(""),  "Cap collection to a size. Use G or M.")
        ("drop", po::bool_switch()->default_value(false),  "Drop collection before inserts.")
        ("threads,t", po::value<int>()->default_value(4),  "Threads to use for streaming to database.")
        ("vertex", po::bool_switch()->default_value(false),  "Create one doc per vertex.")
    
    // camera settings
        ("frames,n", po::value<int>()->default_value(100), "Frames to capture. Zero frames means uninterrupted capture.")
        ("frame-width,w", po::value<int>()->default_value(640), "Frame width.")
        ("frame-height,h", po::value<int>()->default_value(480), "Frame height.")
    
    // Debug settings
        ("half", po::bool_switch()->default_value(false), "Store half the frames captured.")
        ("no-db", po::bool_switch()->default_value(false), "Disable database access.")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help")) {
         BOOST_LOG_TRIVIAL(info)  << desc;
        return 1;
    }

    bool db_enabled = !vm["no-db"].as<bool>();

    MongoStream db( vm["threads"].as<int>());

    if (db_enabled) {
        if (db.initialize( vm["mongo-uri"].as<std::string>(),
                            vm["mongo-db"].as<std::string>(),  
                            vm["mongo-collection"].as<std::string>(),
                            vm["drop"].as<bool>(),
                            vm["capped"].as<std::string>()))  {
            BOOST_LOG_TRIVIAL(fatal) << "Error in db access";
            return 1;
        }
        
        db.setDocsPerVertex(vm["vertex"].as<bool>());
        BOOST_LOG_TRIVIAL(info) << "Namespace: "
                                <<  vm["mongo-db"].as<std::string>()
                                << "."
                                <<   vm["mongo-collection"].as<std::string>();
    }
    else {
        BOOST_LOG_TRIVIAL(info) << "Database access disabled";
    }


  // Create a pipeline to easily configure and start the camera
    rs2::pipeline pipe;
    rs2::config cfg;

    BOOST_LOG_TRIVIAL(info) << "Configure frame size to width: " << vm["frame-width"].as<int>() << " height:" << vm["frame-height"].as<int>();
    cfg.enable_stream(RS2_STREAM_DEPTH, vm["frame-width"].as<int>(), vm["frame-height"].as<int>());
    cfg.enable_stream(RS2_STREAM_COLOR, vm["frame-width"].as<int>(), vm["frame-height"].as<int>());
    
    
    // A coherent set of frames
    pipe.start(cfg);

    rs2::context ctx;
    BOOST_LOG_TRIVIAL(info) << "Realsense2 API version: " << RS2_API_VERSION_STR;
    BOOST_LOG_TRIVIAL(info) << "You have " << ctx.query_devices().size() << " RealSense devices connected" ;

    
    // Declare pointcloud object, for calculating pointclouds and texture mappings
    rs2::pointcloud pc;
    // We want the points object to be persistent so we can display the last cloud when a frame drops
    rs2::points points;

    unsigned int total_frames = vm["frames"].as<int>();
    std::vector<rs2::vertex> vertices;

    int64_t start_ms = now_ms();
  
    int64_t total_verts_acquired = 0;

    unsigned frame_count = 0;
    sig_caught = 0;
    signal(SIGINT, handle_sigint);
    
    while ((total_frames == 0 ) || (frame_count<total_frames)) {
    
        // Wait for the next set of frames from the camera
        auto frames = pipe.wait_for_frames();

        auto color = frames.get_color_frame();
        // For cameras that don't have RGB sensor, we'll map the pointcloud to infrared instead of color
        if (!color)
            color = frames.get_infrared_frame();

        // Tell pointcloud object to map to this color frame
        pc.map_to(color);
        
        // Generate the pointcloud and texture mappings
        auto depth = frames.get_depth_frame();
        points = pc.calculate(depth);
          
         ++frame_count;
        
        if ( vm["half"].as<bool>() && (frame_count % 1 == 0) ) continue;
        
        //points.export_to_ply("out.ply", frames);
            
        vertices.clear();
        if (::convert_points(points, vertices))  {

            auto timestamp = points.get_timestamp();
                        
            BOOST_LOG_TRIVIAL(info) << "Frame ("
                << frame_count <<"): "
                << frame_count << "/" << total_frames
                << " Verts: " << vertices.size();
            
            total_verts_acquired += vertices.size();
            
            if (db_enabled)
                db.sendPoints(timestamp, frame_count, vertices);
            
        }

        if (sig_caught) {
            break;
        }
        
       
    }

    BOOST_LOG_TRIVIAL(info) << "Finished acquisition " << total_verts_acquired
                            << " vertices acquired in " << (now_ms()-start_ms) << " milliseconds "
			    << ((1000.0*total_verts_acquired)/(now_ms()-start_ms)) << " v/s";
    
    BOOST_LOG_TRIVIAL(info) << "Flushing frames to database";
    db.flush();
    sleep(2);
    BOOST_LOG_TRIVIAL(info) << "Close database connections.";
    db.close();
            
    return EXIT_SUCCESS;
}
catch (const rs2::error & e) {
    BOOST_LOG_TRIVIAL(error) << "RealSense error calling " << e.get_failed_function() 
                             << "(" << e.get_failed_args() << "):    " << e.what() ;
    return EXIT_FAILURE;
}
catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << e.what();
    return EXIT_FAILURE;
}
