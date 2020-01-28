#include "MongoStream.hpp"

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/program_options.hpp>

#include "utils.hpp"

#include <cstdlib>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/authentication_exception.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <mongocxx/exception/write_exception.hpp>

using namespace mongocxx;
using namespace bsoncxx;

mongocxx::instance   instance{};

void  MongoStream::consumer(unsigned int index) {
  
    using builder::basic::kvp;
    using builder::basic::make_document;


    BOOST_LOG_TRIVIAL(info) << "Start thread (" << index << ") " <<  this_thread::get_id();

    while ( true ) {
        BOOST_LOG_TRIVIAL(debug) << "Worker " << index << " waiting";
        {
            unique_lock<std::mutex> lock(queue_mutex[index]);
            queue_wait[index].wait(lock);

            frame_ptr[index] = worker_queue[index].front();

            if (frame_ptr[index] == nullptr) {
                BOOST_LOG_TRIVIAL(info) <<  "Worker " << index << " ending.";
                return ;
            }
            worker_queue[index].pop();
            lock.unlock();
            queue_wait[index].notify_all();
        }
        
        if (!frame_ptr[index]->get_vertices().empty()) {

            BOOST_LOG_TRIVIAL(info) <<  "Worker " << index 
                                    << " inserting frame " << frame_ptr[index]->get_frame_number()
                                    << ". Vertices count " << frame_ptr[index]->get_vertices().size();

            int64_t start_ms  =  now_ms();

            if (docPerVertex) {
                std::vector<bsoncxx::document::value> documents;
                for (auto const& v: frame_ptr[index]->get_vertices()) {
                    documents.push_back(  make_document(
                                kvp("ts", start_ms),
                                kvp("frame", (int32_t)frame_ptr[index]->get_frame_number()),
                                kvp("p", make_document( kvp("x", v.x), kvp("y", v.y), kvp("z", v.z) ))
                                ) );
             
                }
                auto client = connpool->acquire();
                (*client)[db_name][coll_name].insert_many(documents);
            }
            else {
                
                // Make one document from all vertices.
                // TODO: Since this method can easily create documents bigger than 16MB, we need to implement a way to partition the document many documents, adding a sequential number and total number of docs per frame.
                
                auto doc = builder::basic::document{};
                auto arr = builder::basic::array{};
                
                using bsoncxx::builder::basic::sub_document;
                using bsoncxx::builder::basic::sub_array;
                
                auto vertices = frame_ptr[index]->get_vertices();
                doc.append(kvp("ts", start_ms),
                           kvp("frame", (int32_t)frame_ptr[index]->get_frame_number()),
                           kvp("vertices",
                                    [vertices](sub_array vert_array) {
                                        
                                        for (auto const &v: vertices) {
                                            auto point = builder::basic::array{};
                                            point.append(v.x, v.y, v.z);
                                            vert_array.append(point);
                                        }
                                    }
                            )
                );

                try {
                    auto client = connpool->acquire();
                    (*client)[db_name][coll_name].insert_one(doc.view());
                    }
                catch (mongocxx::exception e) {
                    BOOST_LOG_TRIVIAL(error) << "Worker " << index << ": Error inserting document";
                    return ;
                }

            }
            // report speed
            BOOST_LOG_TRIVIAL(info) << "Worker " << index << ": "
                                    <<  (1000.0 * frame_ptr[index]->get_vertices().size()/ (1.0*(now_ms()-start_ms))) << " docs per second";
            
             delete frame_ptr[index];
        } else {
            BOOST_LOG_TRIVIAL(error) << "Worker " << index << ": vert array size is 0";
	}
   }
}


int MongoStream::initialize(std::string uri, std::string db, std::string coll, bool drop, std::string capped) {
    
    using builder::basic::kvp;
    using builder::basic::make_document;
    
    coll_name = coll;
    db_name = db;

    connpool = new mongocxx::pool{ mongocxx::uri(uri)};

    try {
        auto client = connpool->acquire();
        auto result =  (*client)[db_name].run_command(make_document(kvp("isMaster", 1)));
        BOOST_LOG_TRIVIAL(info) << "Initializing MongoDB " << bsoncxx::to_json(result);
        if (drop) {
            BOOST_LOG_TRIVIAL(info) << "Droping collection " << coll_name;
            (*client)[db_name][coll_name].drop();
        }
        
        if (!capped.empty()) {
            char last = capped.back();
            unsigned value = std::stoi( capped.substr(0, capped.length()-1) );
            
            mongocxx::options::create_collection options;
            options.capped(true);
            
            if (last == 'G' || last == 'g') {
                options.size(1024*1024*1024*value);
            } else if (last == 'M' || last == 'm') {
                options.size(1024*1024*value);
            }
            else {
                BOOST_LOG_TRIVIAL(error) << "Capped parameter needs to specify units (M or G)";
                return 1;
            }
            BOOST_LOG_TRIVIAL(info) << "Creating collection " << coll_name << " capped at " << capped;
            (*client)[db_name].create_collection(coll_name, options);
        }
        
    } catch(mongocxx::authentication_exception auth) {
        
         BOOST_LOG_TRIVIAL(fatal) << "Error initializing MongoDB: Authentication" << auth.what() ;
         return 1;
    } catch (mongocxx::bulk_write_exception bulk_err) {
        BOOST_LOG_TRIVIAL(fatal) << "Error initializing MongoDB: Bulk write "<< bulk_err.what() ;
        return 1;
    } catch(mongocxx::exception err) {
         BOOST_LOG_TRIVIAL(fatal) << "Error initializing MongoDB " << err.what() ;
        return 1;
    }
    
    // Initialize threads
    BOOST_LOG_TRIVIAL(info) << "Max threads " << thread::hardware_concurrency() << ", using " << queue_workers.size() << ".";

    for (int i=0;i<queue_workers.size(); ++i) {
        queue_workers[i] = std::thread(&MongoStream::consumer, this, i);
        queue_workers[i].detach();
        
    }
    return 0;   
}


int MongoStream::sendPoints(int64_t timestamp, int32_t frame_number, std::vector<rs2::vertex> & vertices) {
    // NB: This is the only place the index is incremented
    if ( (++thread_index) == thread_count)
        thread_index = 0;
    
    { // lock and add to queue
       lock_guard<mutex> lock(queue_mutex[thread_index]);
       worker_queue[thread_index].push(new Frame(vertices, frame_number, timestamp));
    }
    queue_wait[thread_index].notify_all();
    return 0;
}


int MongoStream::getQueueSize(int index)
{
    size_t queueSize;
    { // lock and add to queue
        lock_guard<mutex> lock(queue_mutex[index]);
        queueSize = worker_queue[index].size();
    }
    queue_wait[index].notify_all();
    
    return queueSize;
}


int MongoStream::flush() {
    
    int framesToGo;
    do {
        framesToGo = 0;
              
        for (int index=0; index<thread_count; ++index) {
            
            int queueSize = getQueueSize(index);
            framesToGo += queueSize;
        }
       
        //BOOST_LOG_TRIVIAL(info) << "Frames remaining " << framesToGo;
        
    } while (framesToGo > 0);
    
    return 0;
}


int MongoStream::close() {
    BOOST_LOG_TRIVIAL(info) << "Ending threads.";
    for (int i=0;i<thread_count; ++i) {
        {
           lock_guard<mutex> lock(queue_mutex[i]);
           worker_queue[i].push(nullptr);  // null pointer signals threads to end. This can be done cleaner.
        }
        queue_wait[i].notify_all();
    }
    BOOST_LOG_TRIVIAL(info) << "Messaged all threads";
    return 0;
}


MongoStream::~MongoStream() {
    ; // BOOST_LOG_TRIVIAL(info) << "Destroying stream: threads: " << queue_workers.size();
}
 
