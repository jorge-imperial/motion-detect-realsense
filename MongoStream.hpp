#pragma once

#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>

#include "Frame.hpp"
 
using namespace mongocxx;
using namespace bsoncxx;
using namespace std;

class MongoStream  {

    private:
        mongocxx::pool *connpool; 

        void  consumer(unsigned int index);
    
        std::string db_name;
        std::string coll_name;
        
        vector<thread> queue_workers;
        vector<mutex>  queue_mutex;
        vector<queue<Frame*>> worker_queue;
        vector<condition_variable> queue_wait;
    
        vector<Frame*> frame_ptr;
    
        unsigned  thread_index;
        unsigned  thread_count;
        bool docPerVertex;
    
    public:
        MongoStream(const unsigned n_threads=2) :
            queue_workers(vector<thread>(n_threads)),
            queue_mutex(vector<mutex>(n_threads)),
            worker_queue(vector<queue<Frame*>>(n_threads)),
            queue_wait(vector<condition_variable>(n_threads)),
            frame_ptr(vector<Frame*>(n_threads)),
            thread_count(n_threads),
            thread_index(0),
            docPerVertex(false){}
    
        ~MongoStream();
    
        int initialize(std::string uri, std::string db, std::string coll, bool drop, std::string capped);
        int sendPoints(int64_t timestamp, int32_t frame_number, vector<rs2::vertex> & vertices);
        int getQueueSize(int index);
        int flush();
        void setDocsPerVertex(bool vertex) { docPerVertex = vertex; }
        int close();
};
