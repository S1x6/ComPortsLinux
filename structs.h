//
// Created by Vadim K on 04.01.2021.
//

#ifndef COMPORTS_STRUCTS_H
#define COMPORTS_STRUCTS_H

struct Specs_ {
    int timeout;
    char *portName;
    unsigned char *payload;
    int payloadLength;
    int debug;
};

typedef struct Specs_ Specs;

#endif //COMPORTS_STRUCTS_H
