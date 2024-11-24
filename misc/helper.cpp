#include <random>
#include <sstream>
#include <fstream>
#include "helper.h"

using namespace std;

set<string> generated_ids;                                      // Set of random IDs used
vector<string> msgs_dataset;                                 // Array of articles to serve as a data set 
vector<string> topic_dataset;                                   // Array of topics to serve as a data set
int topic_dataset_idx=0;
int msgs_dataset_idx=0;
pthread_mutex_t topic_dataset_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t msgs_dataset_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to generate a random UUID
string generateRandomStringID(int length) {
    string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, chars.size() - 1);

    while(1){
        string result;
        for (int i = 0; i < length; ++i) {
            result += chars[dis(gen)];
        }
        if(generated_ids.find(result) == generated_ids.end()){
            generated_ids.insert(result);
            return result;
        }
    }
    return NULL;
}

// Prepare the data packet to be sent
string encode(string client_id, int role, int action, string encodedData = "") {
    ostringstream oss;

    string id = generateRandomStringID(25);
    oss << ARG_ID << "=" << id << ";";
    oss << ARG_CLIENT_ID << "=" << client_id << ";";
    oss << ARG_ROLE << "=" << role << ";";
    oss << ARG_ACTION << "=" << action << ";";
    oss << ARG_DATA << "=" << encodedData; // << ";";

    return oss.str();
}

// Decode the data packet recieved
map<string, string> decode(string data) {
    istringstream stream(data);                                     // Create a string stream from the input string
    string pair;
    map<string , string> result;

    while (getline(stream, pair, ';')) {                            // Split by semicolon separator
        istringstream pairStream(pair);
        string id;
        string value;

        if (getline(pairStream, id, '=') && getline(pairStream, value)) {  // Extract key and value from each pair (using '=' as delimiter)
            result[id] = value;
        }
    }
    return result;
}

bool prepDatasets() {
    ifstream topics(FILE_TOPICS);
    ifstream msgs(FILE_MSGS);

    // Check if topics file opened successfully
    if (!topics.is_open()) {
        printf("Error: Could not open file %s\n", FILE_TOPICS);
        return false;
    }

    // Check if msgs file opened successfully
    if (!msgs.is_open()) {
        printf("Error: Could not open file %s\n", FILE_MSGS);
        return false;
    }

    string line;

    pthread_mutex_lock(&topic_dataset_mutex);
    pthread_mutex_lock(&msgs_dataset_mutex);

    // Read file line by line
    while (getline(topics, line)) {
        topic_dataset.push_back(line);
    }

    while (getline(msgs, line)) {
        msgs_dataset.push_back(line);
    }

    pthread_mutex_unlock(&topic_dataset_mutex);
    pthread_mutex_unlock(&msgs_dataset_mutex);

    topics.close();
    msgs.close();
    return true;
}

string getdata(){
    return topic_dataset[0]+ " - " + msgs_dataset[0];
}