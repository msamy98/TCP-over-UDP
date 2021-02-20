/*
** listener.c -- a datagram sockets "server" demo
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


using namespace std;

//#define MYPORT "4950"    // the port users will be connecting to

#define MAXBUFLEN 600
#define MAX_DATA_SIZE 500
#define TIMEOUT 2



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

static const int PACKET_HEADER_SIZE = sizeof(ack_packet); 
vector<packet> make_packets(string file_content);
void *get_in_addr(struct sockaddr *sa);
vector<string> read_from_file(string path);
void sending_packets(vector<packet>& packets, int clientfd,struct addrinfo *p,struct sockaddr_storage their_addr);
bool packet_wiil_be_sent(double probability_of_loss);

void sigchld_handler(int s);

int main(void)
{
    int sockfd, newfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    struct sigaction sa;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET_ADDRSTRLEN];

    string server_in_path = "server.in";
    vector<string> lines = read_from_file(server_in_path);

    string my_port = lines[0];
    char* port = &my_port[0];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET6 to use IPv6
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    int yes = 1;
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  { 
        cout << "listener: failed to bind socket\n";
		exit(1);
	}


    sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

    printf("listener: waiting to recvfrom...\n");

    while(true){
        memset(buf,0,MAXBUFLEN);

        addr_len = sizeof their_addr;
        numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
            (struct sockaddr *)&their_addr, &addr_len);
        if (numbytes == -1) {
            perror("recvfrom");
            exit(1);
        }


        printf("listener: got packet from %s\n", inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof s));
        printf("listener: packet is %d bytes long\n", numbytes);
        buf[numbytes] = '\0';
        printf("listener: packet contains \"%s\"\n", buf);

        packet request_packet;
        for(int i=0 ; i<numbytes ; i++) 
            request_packet.data[i] = buf[i];
       // memcpy(&request_packet, buf, numbytes);
        
        if(request_packet.len > sizeof(ack_packet)){
            while(!fork()){
                cout << "----------------Child Server-----------------" << endl;
                close(sockfd);
                if((newfd = socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol)) == -1){
                    perror("listener: socket");
                    exit(1);
                }

                int requested_file_name_length = numbytes;
                string requested_file_name = string(request_packet.data, 0 ,requested_file_name_length);
                
                
                cout << "Listener: file name: " << requested_file_name << endl;
                vector<string> file_contents = read_from_file(requested_file_name);
                cout << "read completed" << endl;
                string file_string_contnets = "";
                for(auto& line:file_contents){
                    file_string_contnets = file_string_contnets + line + "\n";
                }
                //cout << file_string_contnets << endl;
                

                vector<packet> packets = make_packets(file_string_contnets);
                cout << "packets maked" << endl;
                sending_packets(packets,newfd,p,their_addr);
                cout << "----------------------------------------------------" << endl;
                exit(0);
            }
        }
        close(newfd);
    }
    return 0;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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

vector<packet> make_packets(string file_content){
   
    vector<packet> packets;
    packets.reserve((file_content.size() / MAX_DATA_SIZE) + 1);

    char fileContentArray[file_content.size()];
    copy(file_content.begin(), file_content.end(), fileContentArray);
     
    int bytesLeft = file_content.size();
    int iterator = 0;
    uint32_t sequenceNumber = 0;

    while (bytesLeft > 0) {

        int bytesToSend = min((int)MAX_DATA_SIZE, bytesLeft);

        struct packet packet{0, static_cast<uint16_t>(bytesToSend + PACKET_HEADER_SIZE),sequenceNumber, ' '};

        memcpy(packet.data, fileContentArray + iterator, bytesToSend);
        
        packets.push_back(packet);

        bytesLeft -= bytesToSend;
        iterator += bytesToSend;
        sequenceNumber+= bytesToSend;
    }

    return packets;
}

void sending_packets(vector<packet> &packets, int clientfd,struct addrinfo *p,struct sockaddr_storage client_addr){

    string packets_count = to_string(packets.size());
    packet file_info_ack = {0, static_cast<uint16_t>(PACKET_HEADER_SIZE + packets_count.size()),
                                packets[0].seqno - 1, ' '};
    copy(packets_count.begin(),packets_count.end(),file_info_ack.data);

    char *buf = new char[MAXBUFLEN];
    memset(buf,0,MAXBUFLEN);
    //cout<< "before" << endl;
    memcpy(buf, (const unsigned char *) &file_info_ack, file_info_ack.len);
    //cout<< file_info_ack.data << endl;
    int bytes_of_file_info_packet = sendto(clientfd, buf, MAXBUFLEN-1 , 0,(struct sockaddr *)&client_addr, sizeof(struct sockaddr));
    if(bytes_of_file_info_packet == -1){
        perror("sendto");
        exit(1);
    }
    //cout<< "after" << endl;
    
    for(int i=0 ; i<packets.size() ; i++){
       // cout << "packets data\n" << packets.at(i).data << endl;
        packet packet = packets.at(i);
        
        memset(buf,0,MAXBUFLEN);
        memcpy(buf, (const unsigned char *) &packet, packet.len);
        //for(int j=0 ; j<sizeof(packets.at(i)); j++) 
          //  buf[j] = packets.at(i).data[j];
        //cout << "data of packet: "<< i << "is " << buf <<endl;
        if(packet_wiil_be_sent(0.1)){
            cout << "data of packet: "<< i << " is sent"<<endl;
            int bytes_sent = sendto(clientfd,buf,MAXBUFLEN,0,(struct sockaddr*)&client_addr, sizeof(struct sockaddr));
            if(bytes_sent == -1){
                perror("sendto");
                exit(1);
            }
        }else{
            cout << "data of packet: "<< i << " is not sent"<<endl;
        }
            //cout <<"before recieving ack for packet"<< i <<endl;
            memset(buf,0, 600);
            socklen_t addr_len = sizeof client_addr;

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 500000;
            setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) ;
           
		
            if (int bytes_received = recvfrom(clientfd, buf, 600, 0, (struct sockaddr*)&client_addr,  &addr_len) == -1){
                cout << "listener: The last packet may be lost or corrupted .. Resending" << endl;
                i--;
            }
            cout <<"listener: Received ack for packet "<< i <<endl;
        //}


    }
}

bool packet_wiil_be_sent(double probability_of_loss){
    return rand() % 100 > probability_of_loss * 100;
}

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}