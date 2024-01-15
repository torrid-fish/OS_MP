#include <pthread.h>
#include <unistd.h>

#include <iostream>
#include <vector>

#include "consumer.hpp"
#include "item.hpp"
#include "transformer.hpp"
#include "ts_queue.hpp"

#ifndef CONSUMER_CONTROLLER
#define CONSUMER_CONTROLLER

class ConsumerController : public Thread {
   public:
    // constructor
    ConsumerController(
        TSQueue<Item*>* worker_queue,
        TSQueue<Item*>* writer_queue,
        Transformer* transformer,
        int check_period,
        int low_threshold,
        int high_threshold);

    // destructor
    ~ConsumerController();

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

ConsumerController::ConsumerController(
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

ConsumerController::~ConsumerController() {}

void ConsumerController::start() {
    // TODO: starts a ConsumerController thread
    pthread_create(&t, 0, ConsumerController::process, (void*)this);
    time_stamp = 0;
}

void* ConsumerController::process(void* arg) {
    // TODO: implements the ConsumerController's work
    ConsumerController* cc = (ConsumerController*)arg;  // consumer controller

    while (true) {
        if (cc->worker_queue->get_size() > cc->high_threshold) {
            Consumer* consumer = new Consumer(cc->worker_queue, cc->writer_queue, cc->transformer);
            cc->consumers.push_back(consumer);
            consumer->start();
            printf("Scaling up consumers from %d to %d\n", cc->consumers.size() - 1, cc->consumers.size());
        }

        if (cc->worker_queue->get_size() < cc->low_threshold && cc->consumers.size() > 1) {  // at least one consumer
            cc->consumers.back()->cancel();
            cc->consumers.pop_back();
            printf("Scaling down consumers from %d to %d\n", cc->consumers.size() + 1, cc->consumers.size());
        }

        sleep(cc->check_period / 1000000);
    }

    return nullptr;
}

#endif  // CONSUMER_CONTROLLER_HPP
