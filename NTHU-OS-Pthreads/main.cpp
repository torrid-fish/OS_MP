#include <assert.h>
#include <stdlib.h>
#include "ts_queue.hpp"
#include "item.hpp"
#include "reader.hpp"
#include "writer.hpp"
#include "producer.hpp"
#include "consumer_controller.hpp"

#define READER_QUEUE_SIZE 200
#define WORKER_QUEUE_SIZE 200
#define WRITER_QUEUE_SIZE 4000
#define CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE 20
#define CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE 80
#define CONSUMER_CONTROLLER_CHECK_PERIOD 1000000

int main(int argc, char** argv) {
	assert(argc == 4);

	int n = atoi(argv[1]);
	std::string input_file_name(argv[2]);
	std::string output_file_name(argv[3]);

	// TODO: implements main function
	TSQueue<Item*>* q1;
	TSQueue<Item*>* q2;
	TSQueue<Item*>* q3;

	q1 = new TSQueue<Item*>(READER_QUEUE_SIZE); // Input Queue
	q2 = new TSQueue<Item*>(WORKER_QUEUE_SIZE); // Worker Queue
	q3 = new TSQueue<Item*>(WRITER_QUEUE_SIZE); // Writer Queue

	Transformer* transformer = new Transformer;

	Reader* reader = new Reader(n, input_file_name, q1);
	Writer* writer = new Writer(n, output_file_name, q3);

	Producer* p1 = new Producer(q1, q2, transformer);
	Producer* p2 = new Producer(q1, q2, transformer);
	Producer* p3 = new Producer(q1, q2, transformer);
	Producer* p4 = new Producer(q1, q2, transformer);

	ConsumerController * cc = new ConsumerController(
	q2, 
	q3, 
	transformer, 
	CONSUMER_CONTROLLER_CHECK_PERIOD, 
	CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE * WORKER_QUEUE_SIZE / 100, 
	CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE * WORKER_QUEUE_SIZE / 100);
	
	reader->start();
	writer->start();
	cc->start();

	p1->start();
	p2->start();
	p3->start();
	p4->start();
	
	reader->join();
	writer->join();

	delete p1;
	delete p2;
	delete p3;
	delete p4;
	delete cc;
	delete writer;
	delete reader;
	delete transformer;
	delete q1;
	delete q2;
	delete q3;

	return 0;
}