#include <pthread.h>
#include <unistd.h>

#include <iostream>
#include <vector>

#include "consumer.hpp"
#include "item.hpp"
#include "transformer.hpp"
#include "ts_queue.hpp"

#ifndef CONSUMER_CONTROLLER_TEST
#define CONSUMER_CONTROLLER_TEST

class ConsumerControllerTest : public Thread {
   public:
    // constructor
    ConsumerControllerTest(
        TSQueue<Item*>* worker_queue,
        TSQueue<Item*>* writer_queue,
        Transformer* transformer,
        int check_period,
        int low_threshold,
        int high_threshold);

    // destructor
    ~ConsumerControllerTest();

    // update time stamp
    void updateTimeStamp() {
        time_stamp += 10000;
    }

    // get time stamp
    long long int getTimeStamp() {
        return time_stamp;
    }

    virtual void start();

   private:
    std::vector<Consumer*> consumers;

    TSQueue<Item*>* worker_queue;
    TSQueue<Item*>* writer_queue;

    Transformer* transformer;

    // Check to scale down or scale up every check period in microseconds.
    int check_period;
    // When the number of items in the worker queue is lower than low_threshold,
    // the number of consumers scaled down by 1.
    int low_threshold;
    // When the number of items in the worker queue is higher than high_threshold,
    // the number of consumers scaled up by 1.
    int high_threshold;
    // Use to log the time of action
    long long int time_stamp;

    static void* process(void* arg);
};

// Implementation start

ConsumerControllerTest::ConsumerControllerTest(
    TSQueue<Item*>* worker_queue,
    TSQueue<Item*>* writer_queue,
    Transformer* transformer,
    int check_period,
    int low_threshold,
    int high_threshold) : worker_queue(worker_queue),
                          writer_queue(writer_queue),
                          transformer(transformer),
                          check_period(check_period),
                          low_threshold(low_threshold),
                          high_threshold(high_threshold) {
}

ConsumerControllerTest::~ConsumerControllerTest() {}

void ConsumerControllerTest::start() {
    // TODO: starts a ConsumerControllerTest thread
    pthread_create(&t, 0, ConsumerControllerTest::process, (void*)this);
    time_stamp = 0;
}

void* ConsumerControllerTest::process(void* arg) {
    // TODO: implements the ConsumerControllerTest's work
    ConsumerControllerTest* cc = (ConsumerControllerTest*)arg;  // consumer controller

    while (true) {
        if (cc->getTimeStamp() && cc->getTimeStamp() % cc->check_period == 0) {
            if (cc->worker_queue->get_size() > cc->high_threshold) {
                Consumer* consumer = new Consumer(cc->worker_queue, cc->writer_queue, cc->transformer);
                cc->consumers.push_back(consumer);
                consumer->start();
                
            }

            if (cc->worker_queue->get_size() < cc->low_threshold && cc->consumers.size() > 1) {  // at least one consumer
                cc->consumers.back()->cancel();
                cc->consumers.pop_back();
            }
        }
        std::cout << cc->getTimeStamp() << " " << cc->consumers.size() << " " << cc->worker_queue->get_size() << "\n";
        
        usleep(10000);
        cc->updateTimeStamp();
    }

    return nullptr;
}

#endif  // CONSUMER_CONTROLLER_HPP
