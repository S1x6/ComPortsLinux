#include <stdio.h>
#include "stdlib.h"
#include "structs.h"
#include "time.h"

// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()
#include <string.h>
#include <poll.h>

Specs *readArgs(int, char **);

void printHelpAndExit();

void readHexToBytes(char *, Specs *);

unsigned long writeToPort(int, unsigned char *, int);

void readFromPortAndPrint(int, int);

void printBytesAsHex(unsigned char *bytes, unsigned long length);

void printCurrentTimeWithText(char *);

unsigned long long getCurrentTimeInMs();

int main(int argc, char *argv[]) {
    printf("%d", POLLNVAL);
    printCurrentTimeWithText("Start");
    Specs *specs = readArgs(argc, argv);
    int hComm = open(specs->portName, O_RDWR);

    if (hComm < 0) {
        printf("Error %i from open: %s\n", errno, strerror(errno));
        printHelpAndExit();
    }

    struct termios tty;
    if (tcgetattr(hComm, &tty) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        printHelpAndExit();
    }

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag |= CS8; // transfer 8 bits of data per byte
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL; // reading and disabling sending disconnection signal (?)

    tty.c_lflag &= ~ICANON; // disable reading by lines
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                     ICRNL); // Disable any special handling of received bytes (we need raw)
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed


    // let in be here just in case
//    tty.c_cc[VTIME] = specs->timeout; // Wait for up to specified time in deciseconds (10 = 1s), returning as soon as any data is received.
//    tty.c_cc[VTIME] = 0; // no awaiting
//    tty.c_cc[VMIN] = 1; // there is no minimal amount of bytes required


    // save settings
    if (tcsetattr(hComm, TCSANOW, &tty) != 0) {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        printHelpAndExit();
    }
    fcntl(hComm, F_SETFL, 0);

    printCurrentTimeWithText("Write start");
    unsigned long writtenBytes = writeToPort(hComm, specs->payload, specs->payloadLength);
    printCurrentTimeWithText("Write finish");

    printf("Successfully wrote %lu byte(s)\n", writtenBytes);

    printCurrentTimeWithText("Read start");
    readFromPortAndPrint(hComm, specs->timeout);
    printCurrentTimeWithText("Read finish");

    close(hComm);
    return 0;
}

unsigned long long getCurrentTimeInMs() {
    unsigned long ms; // Milliseconds
    time_t s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s = spec.tv_sec;
    ms = (long) spec.tv_nsec / 1000000; // Convert nanoseconds to milliseconds
    if (ms > 999) {
        s++;
        ms = 0;
    }
    return s * 1000 + ms;
}

void printCurrentTimeWithText(char *text) {
    unsigned long long total = getCurrentTimeInMs();

    printf("%s: %lld s %lld ms\n", text, total / 1000, total % 1000);
}

unsigned long writeToPort(int port, unsigned char *payload, int payloadLength) {
    unsigned long bytesWritten = write(port, payload, payloadLength);
    return bytesWritten;
}

void readFromPortAndPrint(int port, int timeout) {
    unsigned char readBuf[4096];

    int bytesRead = 0;
    int ret = 1;

    struct timeval tv;
    fd_set rfds;
    while (ret > 0) {
        FD_ZERO(&rfds);
        FD_SET(port, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = timeout * 1000; // millisec to microsec

        ret = select(port + 1, &rfds, NULL, NULL, &tv);

        if (ret == -1) { printf("error"); }
        else if (ret == 0) {
            break;
        } else {
            if (FD_ISSET(port, &rfds)) {
                // reading
                unsigned char *buf = readBuf + bytesRead; // move the point to where should read
                int res = read(port, &buf, sizeof(readBuf));
                printf("Read %d bytes\n", bytesRead);
                if (res < 0) {
                    printf("Error %i from read: %s\n", errno, strerror(errno));
                    exit(0);
                }
                bytesRead += res;
            }
        }
    }

    printf("Successfully read %d byte(s)\n", bytesRead);
    printBytesAsHex(readBuf, bytesRead);
}

void printBytesAsHex(unsigned char *bytes, unsigned long length) {
    printf("Response: ");
    for (int i = 0; i < length; i++) {
        printf("%02X", bytes[i]);
    }
    printf("\n");
}

Specs *readArgs(int argc, char *argv[]) {
    Specs *specs = (Specs *) malloc(sizeof(Specs));
    specs->payload = NULL;
    specs->payloadLength = 0;
    specs->timeout = -1;
    specs->portName = NULL;

    for (int i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (strcmp(arg, "-t") == 0) {
            ++i;
            if (argc <= i) printHelpAndExit();
            specs->timeout = atoi(argv[i]);
        } else if (strcmp(arg, "-p") == 0) {
            ++i;
            if (argc <= i) printHelpAndExit();
            specs->portName = (char *) malloc(sizeof(char) * strlen(argv[i]) + 1 + 8); // for \0 and /dev/tty
            strcpy(specs->portName, "/dev/tty");
            strcpy(specs->portName + 8, argv[i]);
        } else if (strcmp(arg, "-w") == 0) {
            ++i;
            if (argc <= i) printHelpAndExit();
            readHexToBytes(argv[i], specs);
        } else printHelpAndExit();
    }

    if (specs->payload == 0 || specs->portName == NULL || specs->timeout == -1) printHelpAndExit();
    return specs;
}

void printHelpAndExit() {
    printf("args (all required):\n\t-t\tвремя ожидания ответа в мс\n\t-p\tимя порта\n\t-w\tотправляемые hex данные (например 00ABC8DF)\n");
    exit(0);
}

void readHexToBytes(char *hex, Specs *specs) {
    int length = strlen(hex) / 2;
    unsigned char *val = (unsigned char *) malloc(length);
    const char *pos = hex;

    // no sanitization or error-checking whatsoever
    for (size_t count = 0; count < length; count++) {
        sscanf(pos, "%2hhX", &val[count]);
        pos += 2;
    }

    specs->payload = val;
    specs->payloadLength = length;
}
