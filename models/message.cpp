#include <stdlib.h>
#include <string>
#include <set>
#include <sstream>
#include "../misc/helper.h"

using namespace std;

#define PRIORITY_HIGH   "high"
#define PRIORITY_LOW    "low"

struct Message {
    string id;
    string value;
    bool priority;
    string topic;
};

Message createMessage(string topic, bool priority = false) {
    Message msg;
    pthread_mutex_lock(&msgs_dataset_mutex);
    int idx = (msgs_dataset_idx++) % msgs_dataset.size();
    pthread_mutex_unlock(&msgs_dataset_mutex);
    msg.id = generateRandomStringID(20);
    msg.value = msgs_dataset[idx];
    msg.priority = priority;
    msg.topic = topic;
    return msg;
}

string encodeMessage(Message message){
    ostringstream oss;                                              // Using ostringstream for easy string concatenation
    string priority = message.priority ? PRIORITY_HIGH : PRIORITY_LOW;
    oss << message.id << ":";
    oss << message.value << ":";
    oss << priority << ":";
    oss << message.topic; // << ":";
    
    return oss.str();                                               // Return the concatenated string
}

Message decodeMessage(string data) {
    istringstream stream(data);
    Message message;
    string priority;

    getline(stream, message.id, ':');
    getline(stream, message.value, ':');
    getline(stream, priority, ':');
    getline(stream, message.topic);
    message.priority = (priority == PRIORITY_HIGH);

    return message;
}