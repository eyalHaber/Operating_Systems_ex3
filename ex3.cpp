// Eyal Haber 203786298

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <queue>
#include <map>
#include <semaphore.h>
#include <mutex>
#include <thread>

using namespace std;

// functions and classes:
class BoundedQueue; // class constructor
vector<string> splitString(string str, char splitter);
void Producer(int id, int stories);
void Dispatcher();
void Co_Editor(int type);

// global variables:
int Co_Editor_queue_size;
map<int, BoundedQueue*> producers_Queues_map; // key = producer's id: value = unique BoundedQueue
BoundedQueue *shared_queue; // Co-Editors shared bounded queue for the Screen Manager
string story_types[] = {" NEWS ", " SPORTS ", " WEATHER "};
queue<string> unbounded_Queues[3]; // the three Co-Editor queues

// class constructor:
class BoundedQueue {
private:
    queue<string> buffer; // bounded buffer queue
    sem_t free{}; // semaphore for free space in buffer
    sem_t full{}; // semaphore for full buffer
    mutex mtx; // mutex for critical section

public:
    BoundedQueue(int size) { // initiate Bounded queue with semaphores
        sem_init(&this->free, 0, size); // buffer is not full (free space available)
        sem_init(&this->full, 0, 0); // buffer is full (free space not available)
    }
    virtual ~BoundedQueue() { // destroys the initiated semaphores
        //destroy the semaphores
        sem_destroy(&this->free);
        sem_destroy(&this->full);
    }
    void insert(string story) { // insert new story to the buffer
        sem_wait(&this->free); // wait if full (locks the semaphore)
        this->mtx.lock(); // The calling thread locks the mutex
        this->buffer.push(story);
        this->mtx.unlock(); // unlock mutex
        sem_post(&this->full); // unlocks the semaphore
    }
    string remove() {
        sem_wait(&this->full); // wait if empty (locks the semaphore)
        this->mtx.lock(); // The calling thread locks the mutex
        string story = this->buffer.front();
        this->buffer.pop();
        this->mtx.unlock(); // unlock mutex
        sem_post(&this->free); // unlocks the semaphore
        return story;
    }
};

int main(int argc, char* argv[]) {
    // Let's initiate the broadcast system
    //
    // first, let's receive and read the Configuration File:
    if (argc < 2) { // if user didn't enter config file
        cout << "configuration file was not entered!" << endl;
        return -1;
    } // if user entered config file:
    for (int i = 0; i < 3; i++) { // enter 'regular' unbounded queues for CO-Editors
        unbounded_Queues[i] = queue<string>();
    }
    vector<thread> threads; // vector of all N+4 threads in the mechanism
    ofstream();
    string config_file = argv[1];
    ifstream file(config_file);
    //
    // read lines of the Configuration File:
    string line;
    int flag = 0;  // flag for each producer being created (0->1->2)
    int id;  // producers ID
    int stories; // number of stories created by this producer
    int queue_size; // bounded queue buffer size
    //
    while (getline(file, line)) {
        // parse line to individual words divided by ' ':
        vector<string> words = splitString(line, ' ');
        // check the words:
        if (words[0] == "PRODUCER") { // this is a new producer!
            flag = 1;  // let's initiate a producer object
            // now we need to create a new producer
            id = stoi(words[1]); // this is the current producer's id
        }
        if (words[0] == "queue" && flag == 2) { // current producer's bounded queue size
            queue_size = stoi(words[3]); // BOUNDED QUEUE!
            // NOW WE NEED TO INITIATE THE QUEUE FOR THIS PRODUCER:
            producers_Queues_map[id] = new BoundedQueue(queue_size);
            threads.emplace_back(Producer, id, stories);
            flag = 0;  // finished initiating current producer!
        }
        if (words[0] == "Co-Editor") {  // arrived the last line of the file
            Co_Editor_queue_size = stoi(words[4]);  // BOUNDED QUEUE!
        }
        if (words[0] != "PRODUCER" && flag == 1) {
            stories = stoi(words[0]);
            flag = 2;
        }
    }
    // initiate Dispatcher:
    threads.emplace_back(Dispatcher);
    // initiate Co-Editors:
    BoundedQueue shared_bounded_queue(Co_Editor_queue_size);
    shared_queue = &shared_bounded_queue;
    for (int i = 0; i < 3; i++) {
        threads.emplace_back(Co_Editor, i); // create Co-Editor threads
    }
    // initiate Screen Manager after the three Co-Editors:
    int counter = 0; // count until 3 'DONE' from Co-Editors
    while (counter < 3) {
        string story = shared_queue->remove();
        if (story == "DONE") {
            counter++;
        }
        else {
            cout << story << endl;
        }
    }
    cout << "DONE" << endl;
    for (auto &thread : threads) { // blocking threads until the first thread execution process is completed
        thread.join();
    }
    for (auto queue : producers_Queues_map) {
        delete queue.second; // delete all bounded producer queue
    }
    file.close();
    return 0;
}

// functions:

vector<string> splitString(string str, char splitter){
    vector<string> result;
    string current = "";
    for(int i = 0; i < str.size(); i++){
        if(str[i] == splitter){
            if(current != ""){
                result.push_back(current);
                current = "";
            }
            continue;
        }
        current += str[i];
    }
    if(current.size() != 0)
        result.push_back(current);
    return result;
}

//  synchronization mechanism system "pieces":

void Producer(int id, int stories) { // producer creates stories
    int story_type_counters[] = {0, 0, 0};  // count number of story types produced
    for (int i = 0; i < stories; i++) {
        int type = rand() % 3; // modulo
        string story = "Producer " + to_string(id) + story_types[type] + to_string(story_type_counters[type]);
        story_type_counters[type]++; // increase type counter from story creates above
        producers_Queues_map[id]->insert(story); // insert the story to the producer's queue
    }
    producers_Queues_map[id]->insert("DONE"); // producer finished to produce new stories
}

void Dispatcher() { // Dispatcher 'collect' stories from producers and sends them to 3 different Co-Editors
    bool finished = false;  // still collecting stories from producers
    while (not finished) {
        finished = true;
        for (auto &producers_Queue : producers_Queues_map) {  // loop on the producers Queues
            string story = producers_Queue.second->remove(); // remove producer's queue
            if (story == "DONE") { // current producer's queue is full
                producers_Queue.second->insert("DONE");
                continue;
            }
            finished = false;
            for (int i = 0; i < 3; i++) {
                size_t count = story.find(story_types[i]);
                if (count != string::npos) { // Maximum value for size_t count
                    unbounded_Queues[i].push(story); // push the right story to the matched queue
                    break;
                }
            }
        }
    }
    // finished sending all stories to all Co-editors
    for (auto &unbounded_Queue : unbounded_Queues) { // loop on the unbounded_ Queues
        unbounded_Queue.push("DONE");
    }
}

void Co_Editor(int type) { // unbounded queue for specific (S, N, W) type
    bool co_editing = true; // still Co-Editing
    while (co_editing) {
        if (unbounded_Queues[type].empty()) {
            continue;
        }
        string story = unbounded_Queues[type].front();
        unbounded_Queues[type].pop();
		// The editing process will be simulated by the Co-Editors by blocking for one tenth (0.1) of a second (=100 milliseconds)
		this_thread::sleep_for(chrono::milliseconds(100)); // waiting threads
        shared_queue->insert(story);
        if (story == "DONE") { // finished C0-Editing
            co_editing = false;
        }
    }
}