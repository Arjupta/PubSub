#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <assert.h>
#include <vector>
#include "../models/topic.cpp"
#include "../models/message.cpp"

#define BUF_SIZE 1024 * 10

set<string> myTopics;
vector<string> fetchedTopics;

double total_time = 0, completed_req = 0;
int serv_sock;
bool keep_running = true;
pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t msg_topic__mutex = PTHREAD_MUTEX_INITIALIZER;

double GetTime() {
    struct timeval t;
    int rc = gettimeofday(&t, NULL);
    assert(rc == 0);
    return (double) t.tv_sec + (double) t.tv_usec/1e6;
}

string registerSubMsg(){
    string pid = to_string(getpid());
    return encode(pid, ROLE_SUBSCRIBER, ACT_UNREGISTER_SUB, "");
}

string unregisterSubMsg(){
    string pid = to_string(getpid());
    return encode(pid, ROLE_SUBSCRIBER, ACT_UNREGISTER_SUB, "");
}

string subscribeTopicMsg(){
    string pid = to_string(getpid());
    
    pthread_mutex_lock(&msg_topic__mutex);
    
    string topic;
    do {
        int idx = rand() % fetchedTopics.size();
        topic = fetchedTopics[idx];
    } while(myTopics.find(topic) != myTopics.end());
    myTopics.insert(topic);

    pthread_mutex_unlock(&msg_topic__mutex);
    
    return encode(pid, ROLE_SUBSCRIBER, ACT_SUBSCRIBE_TOPIC, topic);
}

string unsubscribeTopicMsg(){
    string pid = to_string(getpid());
     
    pthread_mutex_lock(&msg_topic__mutex);
    
    string topic;
    int idx = rand() % myTopics.size();
    set<string>::iterator it = myTopics.begin();
    advance(it, idx);
    topic = *it;
    
    myTopics.erase(topic);

    pthread_mutex_unlock(&msg_topic__mutex);
    
    return encode(pid, ROLE_SUBSCRIBER, ACT_UNSUBSCRIBE_TOPIC, topic);
}

string fetchTopicsMsg(){
    string pid = to_string(getpid());
    return encode(pid, ROLE_PUBLISHER, ACT_FETCH_TOPICS, "");
}

void* readerThread(void* arg) {
    char buffer[BUF_SIZE];
    while (1) {
        pthread_mutex_lock(&running_mutex);
        if(!keep_running){
            pthread_mutex_unlock(&running_mutex);
            break;
        }
        pthread_mutex_unlock(&running_mutex);

        int ret = recv(serv_sock, buffer, BUF_SIZE, 0);
        if (ret > 0) {
            buffer[ret] = '\0';                                             // Null-terminate the received string
            printf("[SUB] - Received: %s\n", buffer);

            string temp(buffer);
            auto packet = decode(temp);

            switch (stoi(packet[ARG_ACTION])) {
                case ACT_FETCH_TOPICS: {
                    Message msg = decodeMessage(packet[ARG_DATA]);
                    pthread_mutex_lock(&msg_topic__mutex);
                    fetchedTopics.clear();
                    fetchedTopics = decodeTopics(msg.topic);
                    pthread_mutex_unlock(&msg_topic__mutex);
                    break;
                }
                case ACT_PUSH_NOTIFICATION: {
                    Message msg = decodeMessage(packet[ARG_DATA]);
                    printf("[SUB] - Received -> topic: %s , message: %d\n", msg.topic.c_str(), msg.value.c_str());
                    break;
                }
                default: {
                    printf("[SUB] - %s\n", packet[ARG_DATA].c_str());
                    break;
                }
            }
        } else {
            printf("[SUB] - Failed to receive message\n");
            break;
        }
    }
    return NULL;
}

void* writerThread(void* arg) {
    int num_of_requests = *((int *) arg);
    int ret;
    double duration = 0;
    string message;

    for(int n=0; n <num_of_requests; n++){
        double start_time = GetTime();

        int randomAction = rand() %5;

        switch(randomAction){
            case 0:
                message = fetchTopicsMsg();
                ret = send(serv_sock, message.c_str(), strlen(message.c_str()), 0);
                if (ret == -1)
                {
                    printf("[SUB] - Failed to send message\n");
                    return NULL;
                }
                printf("[SUB] - Sent: %s\n", message.c_str());
                completed_req += 1.0;
                break;

            case 1:
                continue; // Skipping the unsubscription of topics as the simulation stops very early due to lackof topics
                if(myTopics.empty()) break;
                message = unsubscribeTopicMsg();
                ret = send(serv_sock, message.c_str(), strlen(message.c_str()), 0);
                if (ret == -1)
                {
                    printf("[SUB] - Failed to send message\n");
                    return NULL;
                }
                printf("[SUB] - Sent: %s\n", message.c_str());
                completed_req += 1.0;
                break;

            default:
                if(myTopics.empty()) break;
                message = subscribeTopicMsg();
                ret = send(serv_sock, message.c_str(), strlen(message.c_str()), 0);
                if (ret == -1)
                {
                    printf("[SUB] - Failed to send message\n");
                    return NULL;
                }
                printf("[SUB] - Sent: %s\n", message.c_str());
                completed_req += 1.0;
                break;
        }
        duration += GetTime() - start_time;
    }
    pthread_mutex_lock(&running_mutex);
    keep_running = false;
    total_time += duration;
    pthread_mutex_unlock(&running_mutex);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("[SUB] - Usage: %s <server_ip> <server_port> <seed> <numder_of_requests>\n", argv[0]);
        return -1;
    }

    // Initialize the dataset
    if(!prepDatasets()){
        printf("[SUB] - Error in loading data set\n");
        return -1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int seed = atoi(argv[3]);
    int num_of_requests = atoi(argv[4]);

    srand(seed);

    int ret;
    struct sockaddr_in serv_addr;

    if ((serv_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("[SUB] - Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    {
        printf("[SUB] - Invalid address/ Address not supported\n");
        return -1;
    }

    if (connect(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("[SUB] - Connection Failed\n");
        return -1;
    }

    char buffer[BUF_SIZE] = {0};

    string message = registerSubMsg();
    printf("[SUB] - %s\n", message.c_str());

    ret = send(serv_sock, message.c_str(), strlen(message.c_str()), 0);

    pthread_t writer_tid,reader_tid;

    // Start threads.
    pthread_create(&writer_tid, NULL, writerThread, &num_of_requests);
    pthread_create(&reader_tid, NULL, readerThread, NULL);

    // Wait for threads to finish.
    pthread_join(writer_tid, NULL);
    pthread_cancel(reader_tid);

    double avg = total_time / completed_req;
    printf("[SUB] - Average response time %f\n s", avg);

    message = unregisterSubMsg();
    ret = send(serv_sock, message.c_str(), strlen(message.c_str()), 0);
    close(serv_sock);
    return 0;
}
