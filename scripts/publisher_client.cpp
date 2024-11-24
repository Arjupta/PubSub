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
#include <cstring>
#include <cerrno>
#include "../models/topic.cpp"
#include "../models/message.cpp"

#define BUF_SIZE 1024 * 10

vector<string> myTopics;

double GetTime() {
    struct timeval t;
    int rc = gettimeofday(&t, NULL);
    assert(rc == 0);
    return (double) t.tv_sec + (double) t.tv_usec/1e6;
}

string registerPubMsg(){
    string pid = to_string(getpid());
    return encode(pid, ROLE_PUBLISHER, ACT_REGISTER_PUB, "");
}

string unregisterPubMsg(){
    string pid = to_string(getpid());
    return encode(pid, ROLE_PUBLISHER, ACT_UNREGISTER_PUB, "");
}

string createTopicMsg(){
    string pid = to_string(getpid());
    string topic = createTopic();
    myTopics.push_back(topic);
    return encode(pid, ROLE_PUBLISHER, ACT_CREATE_TOPIC, topic);
}

string deleteTopicMsg(){
    string pid = to_string(getpid());
    int indexToErase = rand() % myTopics.size();
    string topic = myTopics[indexToErase];
    myTopics.erase(myTopics.begin() + indexToErase);
    return encode(pid, ROLE_PUBLISHER, ACT_DELETE_TOPIC, topic);
}

string publishMessage(){
    string pid = to_string(getpid());
    int topicIdx = rand() % myTopics.size();
    string topic = myTopics[topicIdx];
    bool priority = (topicIdx % 3 == 0);
    Message msg = createMessage(topic, priority);
    return encode(pid, ROLE_PUBLISHER, ACT_DELETE_TOPIC, encodeMessage(msg));
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("[PUB] - Usage: %s <server_ip> <server_port> <seed> <numder_of_requests>\n", argv[0]);
        return -1;
    }

    // Initialize the dataset
    if(!prepDatasets()){
        printf("[PUB] - Error in loading data set\n");
        return -1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int seed = atoi(argv[3]);
    int num_of_requests = atoi(argv[4]);

    srand(seed);

    double total_time = 0;
    int sock, ret;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("[PUB] - Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    {
        printf("[PUB] - Invalid address/ Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("[PUB] - Connection Failed : %s\n",  strerror(errno));
        return -1;
    }

    char buffer[BUF_SIZE] = {0};

    string message = registerPubMsg();
    printf("[PUB] - %s\n", message.c_str());

    ret = send(sock, message.c_str(), strlen(message.c_str()), 0);

    double completed_req = 0;
    
    for(int n=0; n <num_of_requests; n++){
        double start_time = GetTime();

        int randomAction = rand() %5;

        switch(randomAction){
            case 0:
                message = createTopicMsg();
                ret = send(sock, message.c_str(), strlen(message.c_str()), 0);
                if (ret == -1)
                {
                    printf("[PUB] - Failed to send message\n");
                    return -1;
                }
                printf("[PUB] - Sent: %s\n", message.c_str());
                completed_req += 1.0;
                break;

            case 1:
                break; // Skipping the deletion of topics as the simulation stops very early due to lackof topics
                if(myTopics.empty()) break;
                message = deleteTopicMsg();
                ret = send(sock, message.c_str(), strlen(message.c_str()), 0);
                if (ret == -1)
                {
                    printf("[PUB] - Failed to send message\n");
                    return -1;
                }
                printf("[PUB] - Sent: %s\n", message.c_str());
                completed_req += 1.0;
                break;

            default:
                if(myTopics.empty()) break;
                message = publishMessage();
                ret = send(sock, message.c_str(), strlen(message.c_str()), 0);
                if (ret == -1)
                {
                    printf("[PUB] - Failed to send message\n");
                    return -1;
                }
                printf("[PUB] - Sent: %s\n", message.c_str());
                completed_req += 1.0;
                break;
        }

        double duration = GetTime() - start_time;
        total_time += duration;
    }

    double avg = total_time / completed_req;
    printf("[PUB] - Average response time %f\n s", avg);
    
    message = unregisterPubMsg();
    ret = send(sock, message.c_str(), strlen(message.c_str()), 0);

    close(sock);
    return 0;
}
