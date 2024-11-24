#ifndef HELPER_H
#define HELPER_H

#include <string>
#include <set>
#include <map>
#include <vector>

using namespace std;

// Define types of action a packet can perform
#define ACT_REGISTER_PUB        0
#define ACT_REGISTER_SUB        1
#define ACT_CREATE_TOPIC        2
#define ACT_DELETE_TOPIC        3
#define ACT_FETCH_TOPICS        4
#define ACT_SUBSCRIBE_TOPIC     5
#define ACT_UNSUBSCRIBE_TOPIC   6
#define ACT_PUBLISH_MSG         7
#define ACT_PUSH_NOTIFICATION   8
#define ACT_PULL_NOTIFICATION   9
#define ACT_QUERY_RESPONSE     10
#define ACT_UNREGISTER_PUB     11
#define ACT_UNREGISTER_SUB     12


// Define types of role an entitiy can have
#define ROLE_BROKER             0
#define ROLE_PUBLISHER          1
#define ROLE_SUBSCRIBER         2

// Define identifier for packet arguments 
#define ARG_ID                  "id"
#define ARG_CLIENT_ID           "c_id"
#define ARG_ROLE                "role"
#define ARG_ACTION              "action"
#define ARG_DATA                "data"

// Raw dataset files
#define FILE_TOPICS             "./resource/locations.txt"
#define FILE_MSGS               "./resource/news.txt"

// Misc. headers
#define TOPIC_LIST_MSG          "topic_list"

// Extern declaration to avoid multiple definition issues of a global variable
extern set<string> generated_ids;
extern vector<string> msgs_dataset;
extern vector<string> topic_dataset;
extern int topic_dataset_idx;
extern int msgs_dataset_idx;
extern pthread_mutex_t topic_dataset_mutex;
extern pthread_mutex_t msgs_dataset_mutex;

// Function declarations
string generateRandomStringID(int length);

string encode(string client_id, int role, int action, string encodedData);

map<string, string> decode(string data);

bool prepDatasets();

string getdata();

#endif // HELPER_H
