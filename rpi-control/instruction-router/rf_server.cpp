#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <ctime>       // time()
#include <iostream>    // cin, cout, endl
#include <string>      // string, getline()
#include <time.h>      // CLOCK_MONOTONIC_RAW, timespec, clock_gettime()
#include <RF24/RF24.h> // RF24, RF24_PA_LOW, delay()
 
using namespace std;
 
// Radio CE Pin, CSN Pin, SPI Speed
// CE Pin uses GPIO number with BCM and SPIDEV drivers, other platforms use their own pin numbering
// CS Pin addresses the SPI bus number at /dev/spidev<a>.<b>
// ie: RF24 radio(<ce_pin>, <a>*10+<b>); spidev1.0 is 10, spidev1.1 is 11 etc..
#define CSN_PIN 0
#define CE_PIN 22
RF24 radio(CE_PIN, CSN_PIN);

#define USAGE "usage: rf_server <PORT>"

/** Maximum command line length */
#define N 256

/** Default port number */
#define PORT_NUMBER "26124"

/** Port number length */
#define MAX_PORT_SIZE 5

/** Mutex lock for the board */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/** Print out an error message and exit. */
static void fail(char const *message) {
  fprintf(stderr, "%s\n", message);
  exit(EXIT_FAILURE);
}

/** Handle invalid commands */
static inline void invalid(FILE *fp) { fprintf(fp, "Invalid command\n"); }


/** handle a client connection, close it when we're done. */
void *handleClient(void *arg) {
  radio.stopListening(); // put radio in TX mode

  int sock = *((int *)arg);

  // Here's a nice trick, wrap a C standard IO FILE around the
  // socket, so we can communicate the same way we would read/write
  // a file.
  FILE *fp = fdopen(sock, "a+");

  // Prompt the user for a command.
  // fprintf(fp, "cmd> ");

  // Temporary values for parsing commands.
  char line[N + 1];

  while ((fgets(line, N + 1, fp))) {
    // printf("recv: %s\n", line);
    float payload = 1.3;
    char direction;
    int duration;
    bool res;
    if (sscanf(line, "%c %d", &direction, &duration) != 2) {
      // Command not found / invalid
      invalid(fp);
    } else {
      // printf("recv: %c %d\n", direction, duration);
      switch (direction) {
      case 'F':
        printf("Move forward for %d secs\n", duration);
        // res = radio.write(&direction, sizeof(char));
        // res = radio.write(&duration, sizeof(int));
        res = radio.write(&payload, sizeof(payload));
        printf("%b \n", res);
        break;
      case 'L':
        printf("Turn LEFT for %d secs\n", duration);
        // radio.write(&direction, sizeof(char));
        radio.write(&duration, sizeof(int));
        break;
      case 'R':
        printf("Turn RIGHT for %d secs\n", duration);
        // radio.write(&direction, sizeof(char));
        radio.write(&duration, sizeof(int));
        break;
      case 'Q':
        goto end_loop_switch;
      default:
        invalid(fp);
      }
      
    }
  }
  end_loop_switch:;

  // Close the connection with this client.
  fclose(fp);
  return NULL;
}

int main(int argc, char *argv[]) {
  // Prepare a description of server address criteria.
  struct addrinfo addrCriteria;
  memset(&addrCriteria, 0, sizeof(addrCriteria));
  addrCriteria.ai_family = AF_INET;
  addrCriteria.ai_flags = AI_PASSIVE;
  addrCriteria.ai_socktype = SOCK_STREAM;
  addrCriteria.ai_protocol = IPPROTO_TCP;

  // Lookup a list of matching addresses
  struct addrinfo *servAddr;

  if (argc < 2) {
    if (getaddrinfo(NULL, PORT_NUMBER, &addrCriteria, &servAddr))
      fail("Can't get address info");
  } else {
    if (getaddrinfo(NULL, argv[1], &addrCriteria, &servAddr))
      fail("Can't get address info");
  }

  // Try to just use the first one.
  if (servAddr == NULL)
    fail("Can't get address");

  // Create a TCP socket
  int servSock =
      socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
  if (servSock < 0)
    fail("Can't create socket");

  // Bind to the local address
  if (bind(servSock, servAddr->ai_addr, servAddr->ai_addrlen) != 0)
    fail("Can't bind socket");

  // Tell the socket to listen for incoming connections.
  if (listen(servSock, 5) != 0)
    fail("Can't listen on socket");

  // Free address list allocated by getaddrinfo()
  freeaddrinfo(servAddr);

  // Fields for accepting a client connection.
  struct sockaddr_storage clntAddr; // Client address
  socklen_t clntAddrLen = sizeof(clntAddr);

  // while (true) {
  //   // Accept a client connection.
  //   int sock = accept(servSock, (struct sockaddr *)&clntAddr, &clntAddrLen);

  //   // Create new thread to handle client
  //   pthread_t thread;
  //   if (pthread_create(&thread, NULL, handleClient, &sock) != 0)
  //     fail("Can't create a child thread");

  //   // Detach thread so it will free itself
  //   pthread_detach(thread);
  // }

    // ********** Radio Setup *************
 
    // perform hardware check
    if (!radio.begin()) {
        cout << "radio hardware is not responding!!" << endl;
        return 0; // quit now
    }

    // Let these addresses be used for the pair
    uint8_t address[2][6] = {"1Node", "2Node"};
    bool radioNumber = 1; // 0 uses address[0] to transmit, 1 uses address[1] to transmit
 
    // save on transmission time by setting the radio to only transmit the
    // number of bytes we need to transmit a float
    radio.setPayloadSize(sizeof(int)); // float datatype occupies 4 bytes
 
    // Set the PA Level low to try preventing power supply related problems
    // because these examples are likely run with nodes in close proximity to
    // each other.
    radio.setPALevel(RF24_PA_LOW); // RF24_PA_MAX is default.
    // set the TX address of the RX node for use on the TX pipe (pipe 0)
    radio.stopListening(address[radioNumber]);
 
    // // set the RX address of the TX node into a RX pipe
    // radio.openReadingPipe(1, address[!radioNumber]); // using pipe 1
 
    // For debugging info
    // radio.printDetails();       // (smaller) function that prints raw register values
    radio.printPrettyDetails(); // (larger) function that prints human readable data

  // while(true) {
  int sock = accept(servSock, (struct sockaddr *)&clntAddr, &clntAddrLen);
  handleClient(&sock);
  // }

  // Stop accepting client connections (never reached).
  close(servSock);

  return 0;
}
