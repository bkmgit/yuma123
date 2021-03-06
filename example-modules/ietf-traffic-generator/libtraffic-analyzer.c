#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "libtraffic-analyzer.h"
#include "timespec-math.h"

static unsigned char hexchar2byte(char hexchar)
{
    char byte;
    if(hexchar>='0' && hexchar<='9') {
        byte = hexchar - '0';
    } else if (hexchar>='A' && hexchar<='F') {
        byte = hexchar - 'A' + 10;
    } else if (hexchar>='a' && hexchar<='f') {
        byte = hexchar - 'a' + 10;
    } else {
        assert(0);
    }
    return byte;
}

static void hexstr2bin(char* hexstr, uint8_t* data)
{
    unsigned int i;
    unsigned int len;

    len = strlen(hexstr)/2;

    for(i=0;i<len;i++) {
        data[i] = (hexchar2byte(hexstr[i*2])<<4) | (hexchar2byte(hexstr[i*2+1]));
    }
}


void traffic_analyzer_put_frame(traffic_analyzer_t* ta, uint8_t* frame_data, uint32_t frame_len, uint64_t rx_sec, uint32_t rx_nsec)
{
    struct timespec rx_time;
    struct timespec check_last_tx_time;
    struct timespec check_last_latency;
    uint8_t* timestamp;
    unsigned int offset;
    offset = frame_len - 10;
    timestamp = frame_data+offset;

    ta->totalframes++;

    check_last_tx_time.tv_sec = ((uint64_t)timestamp[0]<<40) + ((uint64_t)timestamp[1]<<32) + ((uint64_t)timestamp[2]<<24) + ((uint64_t)timestamp[3]<<16) + ((uint64_t)timestamp[4]<<8) + ((uint64_t)timestamp[5]);
    check_last_tx_time.tv_nsec = ((uint32_t)timestamp[6]<<24) + ((uint32_t)timestamp[7]<<16) + ((uint32_t)timestamp[8]<<8) + ((uint32_t)timestamp[9]);

    ta->last_rx_time.tv_sec = rx_sec;
    ta->last_rx_time.tv_nsec = rx_nsec;

    timespec_sub(&ta->last_rx_time, &check_last_tx_time, &check_last_latency);

    if(check_last_latency.tv_sec!=0) {
        return;
    }

    ta->testframes++;
    memcpy(&ta->testframe.last_latency, &check_last_latency, sizeof(struct timespec));
    memcpy(&ta->testframe.last_tx_time, &check_last_tx_time, sizeof(struct timespec));
    memcpy(&ta->testframe.last_rx_time, &ta->last_rx_time, sizeof(struct timespec));

    if(ta->testframes<=1 || ta->testframe.last_latency.tv_sec>ta->testframe.max_latency.tv_sec || (ta->testframe.last_latency.tv_sec==ta->testframe.max_latency.tv_sec && ta->testframe.last_latency.tv_nsec>ta->testframe.max_latency.tv_nsec)) {
        ta->testframe.max_latency.tv_nsec = ta->testframe.last_latency.tv_nsec;
        ta->testframe.max_latency.tv_sec = ta->testframe.last_latency.tv_sec;
    }
    if(ta->testframes<=1 || ta->testframe.last_latency.tv_sec<ta->testframe.min_latency.tv_sec || (ta->testframe.last_latency.tv_sec==ta->testframe.min_latency.tv_sec && ta->testframe.last_latency.tv_nsec<ta->testframe.min_latency.tv_nsec)) {
        ta->testframe.min_latency.tv_nsec = ta->testframe.last_latency.tv_nsec;
        ta->testframe.min_latency.tv_sec = ta->testframe.last_latency.tv_sec;
    }
}
