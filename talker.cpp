/*
** talker.c -- a datagram "client" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <fstream>
#include <sstream>
#include <map>
#include <poll.h>

//#define SERVERPORT "4950"    // the port users will be connecting to
#define MAXBUFLEN 600
#define TIMEOUT 2

using namespace std;

vector<string> read_from_file(string path);
void recieve_packets(int sockfd,struct addrinfo *servinfo);
void write_into_file();
void send_ack(uint32_t seqno, int sockfd,struct addrinfo *servinfo);

/* Data-only packets */
struct packet {
    /* Header */
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
    /* Data */
    char data [500]; /* Not always 500 bytes, can be less */
};

/* Ack-only packets are only 8 bytes */
struct ack_packet {
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t ackno;
};

map<uint32_t,string> file_segments;
int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    /*if (argc != 4) {
        fprintf(stderr,"usage: talker hostname port_number message\n");
        exit(1);
    }*/

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    

    string client_in_path = "client.in";
    vector<string> lines = read_from_file(client_in_path);

    string port_ip = lines[0];
    string port_num = lines[1];
    string file_path = lines[2];
    cout << port_ip << endl;
    cout << port_num << endl;
    cout << file_path << endl;

    char* server_port = &port_num[0];
    char* server_ip = &port_ip[0];
    char* requested_file_path = &file_path[0];

    if ((rv = getaddrinfo(server_ip, server_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    if ((numbytes = sendto(sockfd, requested_file_path, strlen(requested_file_path), 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }

    

    printf("talker: sent %d bytes to %s\n", numbytes, server_ip);

    
    recieve_packets(sockfd,servinfo);

    write_into_file();

    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}



vector<string> read_from_file(string path) {
    ifstream input_file(path);
    vector<string> lines;
    if (input_file.is_open()) {
        string line;
        while (getline(input_file, line)) {
            lines.push_back(line);
        }
        input_file.close();
    }
    return lines;
}

void recieve_packets(int sockfd,struct addrinfo *servinfo){

char* buf = new char[MAXBUFLEN];
memset(buf,0,MAXBUFLEN);
socklen_t addr_len = sizeof servinfo;
//cout<< "before" << endl;
//recieve first packet contain the requested file info
int bytes_recieved_file_info = recvfrom(sockfd, buf, 600, 0, (struct sockaddr*)&servinfo, &addr_len);
    if(bytes_recieved_file_info == -1){
        perror("recvfrom");
        exit(1);
    }
    //cout<< "after" << endl;

    auto* recieved_file_info_packet = (struct packet*) buf;
    //cout<< "after" << recieved_file_info_packet->data << endl;

int packets_counter = 0;//counter foe recived packets
int packets_num = stoi(recieved_file_info_packet->data);//counter for total packets number
//cout << packets_num << endl;
    while(packets_counter < packets_num){
        //recieve file packets
        char* buf = new char[MAXBUFLEN];
        memset(buf,0,MAXBUFLEN);
        addr_len = sizeof servinfo;

        struct timeval tv;
            tv.tv_sec = 3;
            tv.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) ;

        int bytes_recieved = recvfrom(sockfd, buf, 600, 0, (struct sockaddr*)&servinfo,  &addr_len);
        if(bytes_recieved < 0){
            //perror("recvfrom");
            cout << "no packets recieved from the server ... ABORTING!!" << endl;
            exit(1);
        }

        cout << "packet no:" << packets_counter << " is recieved" << endl;
        

        auto* recieved_packet = (struct packet*) buf;
        uint32_t seqno = recieved_packet->seqno;
        string packet_data = "";
        packet_data = recieved_packet->data;
        //making map with key as sruence number and value as packet to recieve packets in order
        file_segments.insert(make_pair(seqno,packet_data));
        packets_counter ++;

        send_ack(seqno,sockfd,servinfo);
        
    }
}
void write_into_file(){
    cout << "start writing" << endl;
    string file = "outputfile.txt";
    ofstream fout;
    fout.open(file);
    map<uint32_t, string> :: iterator it;
    for (it=file_segments.begin() ; it!=file_segments.end() ; it++){
        fout << it->second;
       // cout << it->second;
    }
    //cout << endl;
    fout.close();
}
void send_ack(uint32_t seqno, int sockfd,struct addrinfo *servinfo){
    struct ack_packet ack;
    ack.cksum = 0;
    ack.len = sizeof(ack);
    ack.ackno = seqno;

    char* buf = new char[600];
    memset(buf,0, 600);
    memcpy(buf, &ack, sizeof(ack));

    int bytesSent = sendto(sockfd, buf, 600, 0,(struct sockaddr *)&servinfo, sizeof(struct sockaddr));
    if (bytesSent == -1) {
        perror("couldn't send the ack");
        exit(1);
    }
}


