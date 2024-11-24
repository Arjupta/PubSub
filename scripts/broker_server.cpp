#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <sys/epoll.h>
#include <fcntl.h>
#include "../models/topic.cpp"
#include "../models/message.cpp"

#define BUF_SIZE 1024 * 10
#define BACKLOG_SIZE 1024                                                   // Put this into an env file
#define MAX_THREADS 1024                                                    // Internally define max threads server wants to handle. Can be kept large enough
#define MAX_EVENTS 1024 * 10
 
int serv_sock, epoll_sock;
string pid;

// pthread_mutex_t topic_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

map<string, int> pid_to_socket;
map<string, set<string>> topic_to_subs;
map<int, vector<Message>> queuedMsgs;                                    // A data structure to enqueue all the messages per subscriber

// Signal handler to shut down the server
void handle_sigint(int sig) {
    printf("[SER] - \nCaught signal %d. Shutting down the server...\n", sig);
    close(serv_sock);                                                       // Close the server listening socket
    close(epoll_sock);
    exit(0);                                                                // Exit the program
}

void executePacket(string raw, int socket){
    auto packet = decode(raw);
    switch(stoi(packet[ARG_ACTION])){
        case ACT_CREATE_TOPIC: {
            acceptTopic(packet[ARG_DATA]);
            break;
        }
        case ACT_DELETE_TOPIC: {
            eraseTopic(packet[ARG_DATA]);
            break;
        }
        case ACT_FETCH_TOPICS: {
            string data = encodeAllTopics();
            epoll_event ev;                                                 // Create epollout event and enter data into the queue
            ev.events = EPOLLOUT;
            ev.data.fd = socket;
            Message m; m.topic = data; m.value = TOPIC_LIST_MSG;
            pthread_mutex_lock(&queue_mutex);
            queuedMsgs[socket].push_back(m);
            epoll_ctl(epoll_sock, EPOLL_CTL_MOD, socket, &ev);
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        case ACT_PUBLISH_MSG: {
            Message msg = decodeMessage(packet[ARG_DATA]);
            for(auto i : topic_to_subs[msg.topic]){
                epoll_event ev;
                ev.events = EPOLLOUT;
                pthread_mutex_lock(&queue_mutex);
                ev.data.fd = pid_to_socket[i];
                queuedMsgs[socket].push_back(msg);
                epoll_ctl(epoll_sock, EPOLL_CTL_MOD, socket, &ev);
                pthread_mutex_unlock(&queue_mutex);
            }
            break;
        }
        case ACT_REGISTER_PUB: {
            pthread_mutex_lock(&queue_mutex);
            pid_to_socket[packet[ARG_CLIENT_ID]] = socket;
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        case ACT_REGISTER_SUB: {
            pthread_mutex_lock(&queue_mutex);
            pid_to_socket[packet[ARG_CLIENT_ID]] = socket;
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        case ACT_UNREGISTER_PUB: {
            pthread_mutex_lock(&queue_mutex);
            pid_to_socket.erase(packet[ARG_CLIENT_ID]);
            epoll_ctl(epoll_sock, EPOLL_CTL_DEL, socket, NULL);
            pthread_mutex_unlock(&queue_mutex);
            close(socket);
            break;
        }
        case ACT_UNREGISTER_SUB: {
            pthread_mutex_lock(&queue_mutex);
            pid_to_socket.erase(packet[ARG_CLIENT_ID]);
            epoll_ctl(epoll_sock, EPOLL_CTL_DEL, socket, NULL);
            pthread_mutex_unlock(&queue_mutex);
            close(socket);
            break;
        }
        case ACT_SUBSCRIBE_TOPIC: {
            pthread_mutex_lock(&queue_mutex);
            topic_to_subs[packet[ARG_DATA]].insert(packet[ARG_CLIENT_ID]);
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        case ACT_UNSUBSCRIBE_TOPIC: {
            string pid = packet[ARG_CLIENT_ID];
            pthread_mutex_lock(&queue_mutex);
            topic_to_subs[packet[ARG_DATA]].erase(pid);
            vector<Message> newQ;
            for(int i=0; i <queuedMsgs[socket].size(); i++){
                if(queuedMsgs[socket][i].topic == packet[ARG_DATA]){
                    continue;
                }
                newQ.push_back(queuedMsgs[socket][i]);
            }
            queuedMsgs[socket].clear();
            queuedMsgs[socket] = newQ;
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        case ACT_PULL_NOTIFICATION: {
            // Not implemented
            break;
        }
        case ACT_PUSH_NOTIFICATION: {
            // Never going to recieve
            break;
        }
        case ACT_QUERY_RESPONSE: {
            // Not used
            break;
        }
        default: break;
    }
}

void* workerThread(void* arg) {
    struct epoll_event events[MAX_EVENTS];
    char buffer[BUF_SIZE];

    while (true) {
        int num_events = epoll_wait(epoll_sock, events, MAX_EVENTS, -1); // Blocking call
        if (num_events == -1) {
            printf("[SER] - Error in epoll_wait\n");
            continue;
        }

        for (int i = 0; i < num_events; ++i) {
            int socket = events[i].data.fd;
            if (events[i].events & EPOLLIN) {
                int valread = recv(socket, buffer, BUF_SIZE, 0);                // Read the message from the client
                if (valread > 0) {
                    buffer[valread] = '\0';                                         // Null-terminate the received string
                    printf("[SER] - Received: %s\n", buffer);
                    string temp (buffer);
                    executePacket(temp, socket);
                } else {
                    printf("[SER] - Failed to receive message\n");
                    pthread_mutex_lock(&queue_mutex);
                    epoll_ctl(epoll_sock, EPOLL_CTL_DEL, socket, NULL);
                    close(socket);                                                  // Close the client socket after communication 
                    pthread_mutex_unlock(&queue_mutex);
                }
            } else if (events[i].events & EPOLLOUT) {
                pthread_mutex_lock(&queue_mutex);
                if(!queuedMsgs[socket].empty()){
                    Message msg = queuedMsgs[socket].front(); 
                    queuedMsgs[socket].erase(queuedMsgs[socket].begin());
                    pthread_mutex_unlock(&queue_mutex);

                    string encodedMsg = encodeMessage(msg);
                    string data;

                    if(msg.value == TOPIC_LIST_MSG){
                        data = encode(pid, ROLE_BROKER, ACT_FETCH_TOPICS, encodedMsg);
                    } else {
                        data = encode(pid, ROLE_BROKER, ACT_PUSH_NOTIFICATION, encodedMsg);
                    }

                    int ret = send(socket, data.c_str(), strlen(data.c_str()), 0);            // Respond with "world"
                    if (ret == -1)
                    {
                        printf("[SER] - Failed to send message\n");
                        pthread_mutex_lock(&queue_mutex);
                        epoll_ctl(epoll_sock, EPOLL_CTL_DEL, socket, NULL);
                        close(socket);                                                  // Close the client socket after communication 
                        pthread_mutex_unlock(&queue_mutex);
                    }
                    printf("[SER] - Sent: %s\n", data.c_str());
                }
                pthread_mutex_lock(&queue_mutex);
                epoll_event ev;
                ev.events = EPOLLIN;
                ev.data.fd = socket;
                pthread_mutex_lock(&queue_mutex);
                epoll_ctl(epoll_sock, EPOLL_CTL_MOD, socket, &ev);
                pthread_mutex_unlock(&queue_mutex);
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("[SER] - Usage: %s <server_ip> <server_port> <thread_pool_size>\n", argv[0]);
        return -1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int thread_pool_size = atoi(argv[3]);
    pid = to_string(getpid());

    if(thread_pool_size > MAX_THREADS){
        printf("[SER] - At max %d threads can be created\n", MAX_THREADS);
        return -1;
    }

    struct sockaddr_in serv_addr;

    // Set up signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);

    // Initialize the dataset
    if(!prepDatasets()){
        printf("[SER] - Error in loading data set\n");
        return -1;
    }

    if ((serv_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("[SER] - Socket creation error: %d\n", serv_sock);
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    int opt = 1; // Set option value
    if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){        // Setting port to be reusable for multiple runs
        perror("Setsockopt failed");
        return -1;
    }

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    {
        printf("[SER] - Invalid address/ Address not supported\n");
        return -1;
    }

    // Bind to the socket witht the server ip
    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[SER] - Bind failed\n");
        return -1;
    }

    // Listen for incoming connections
    if (listen(serv_sock, BACKLOG_SIZE) < 0) {
        printf("[SER] - Listen failed\n");
        return -1;
    }

    int ret, nfds;
    char buffer[BUF_SIZE] = {0};

    // Create epoll instance 
    if((epoll_sock = epoll_create1(EPOLL_CLOEXEC)) < 0){
        printf("[SER] - Epoll create failure\n");
        return -1;
    }

    pthread_t threads[thread_pool_size];
    for (int i = 0; i < thread_pool_size; i++) {
        if (pthread_create(&threads[i], NULL, workerThread, NULL) != 0) {
            perror("Thread creation failed");
            return -1;
        }
    }

    // Infinitely listen on the port for multiple connections
    while (1) {
        struct sockaddr_in client_addr;
        int addrlen = sizeof(client_addr);

        int new_socket = accept(serv_sock, (struct sockaddr *)&client_addr, (socklen_t*)&addrlen); // Accept a new connection
        if (new_socket < 0) {
            printf("[SER] - Accept failed\n");
            continue;                                                       // Skip to the next iteration to accept more connections
        }

        printf("[SER] - Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        struct epoll_event ev;
        ev.data.fd = new_socket;
        ev.events = EPOLLIN;

        pthread_mutex_lock(&queue_mutex);
        if(epoll_ctl(epoll_sock, EPOLL_CTL_ADD, new_socket, &ev) < 0){              // Add the event details for newly added client connection
            printf("[SER] - Epoll add failed: %d\n", new_socket);
            close(new_socket); 
        }
        pthread_mutex_unlock(&queue_mutex);
    }

    for (int i = 0; i < thread_pool_size; i++) {                            // Although server will run indefinitely
        pthread_join(threads[i], NULL);
    }

    return 0;
}
