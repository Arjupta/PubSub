#include <stdlib.h>
#include <string>
#include <map>
#include <sstream>
#include "../misc/helper.h"

using namespace std;

map<string, string> topics;
pthread_mutex_t topic_mutex = PTHREAD_MUTEX_INITIALIZER;

void acceptTopic(string data) {
    istringstream stream(data);                                     // Create a string stream from the input string
    string id, name;

    if (getline(stream, id, ':') && getline(stream, name)) {        // Extract key and value from each pair (using ':' as delimiter)
        pthread_mutex_lock(&topic_mutex);
        topics[id] = name;
        pthread_mutex_unlock(&topic_mutex);
    }
}

void eraseTopic(string data) {
    istringstream stream(data);                                     // Create a string stream from the input string
    string id, name;

    if (getline(stream, id, ':') && getline(stream, name));
    pthread_mutex_lock(&topic_mutex);
    topics.erase(id);
    pthread_mutex_unlock(&topic_mutex);
}

string createTopic() {
    string topicId = generateRandomStringID(20);
    pthread_mutex_lock(&topic_dataset_mutex);
    int idx = (topic_dataset_idx++) % topic_dataset.size();
    pthread_mutex_unlock(&topic_dataset_mutex);
    string topicName = topic_dataset[idx];
    ostringstream oss;                                              // Using ostringstream for easy string concatenation
    oss << topicId << ":" << topicName;
    return oss.str();                                               // Return the concatenated string
}

string encodeAllTopics() {
    ostringstream oss;                                              // Using ostringstream for easy string concatenation

    // Iterate through the map and concatenate all values
    pthread_mutex_lock(&topic_mutex);
    for (auto i : topics) {
        oss << i.first << ":" << i.second;                          // Add the topic id and topic value
        oss << ",";                                                 // Add a comma between values
    }
    pthread_mutex_unlock(&topic_mutex);
    return oss.str();                                               // Return the concatenated string
}

vector<string> decodeTopics(string data) {
    istringstream stream(data);                                     // Create a string stream from the input string
    string pair;
    vector<string> result;

    while (getline(stream, pair, ',')) {                            // Split by comma separator
        result.push_back(pair);
    }
    return result;
}
