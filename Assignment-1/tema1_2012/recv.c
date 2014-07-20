/* Constantin Serban-Radoi 323CA
 * Tema 1 Protocoale de Comunicatii
 * Martie 2012
 *
 * Reciever
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"
#include "crc.h"

#define HOST "127.0.0.1"
#define PORT 10001

#define PAYLOAD_SZ 1400
#define PCK_LOAD_SZ (PAYLOAD_SZ - (sizeof(int) + sizeof(short int)))
#define CRC_LOAD_SZ (PAYLOAD_SZ + (sizeof(int) + sizeof(short int)))

typedef union _charge {
    /* The original struct */
    struct {
        int type;
        int len;
        char payload[PAYLOAD_SZ];   //1400
    }msg;
    /* Struct having id and crc for packages */
    struct {
        int type;
        int len;
        unsigned int id;
        char load[PCK_LOAD_SZ];     //1394
        word crc;
    }pack;
    /* Struct used to easily compute the crc for message */
    struct {
        char payload[CRC_LOAD_SZ];  //1406
        word crc;
    }crc;
}charge;

/* CRC Table */
word *tabel;

/* Compute CRC checksum */
void compcrc( char *data, int len, word *acum ){
    *acum = 0;
    int i;
    for(i = 0; i < len; ++i)
        crctabel(data[i], acum, tabel);
}

/* Returns window parameter */
int get_window(char* param) {
    int p;
    sscanf(param, "window=%d", &p);
    return p;
}

int main(int argc, char** argv) {
    init(HOST, PORT);
    char filename[1400], filesize[1400];
    int fs;

    /* Read window parameter */
    int window = get_window( argv[1] );
    int win;

    /* Compute the CRC table */
    tabel = tabelcrc(CRCCCITT);

    /* Receive handshake message
     ****************************
     */
    charge *r = NULL;
    r = (charge *)receive_message();

    if (!r){
        perror("Receive message");
        return -1;
    }

    char fn[2000];

    while (r) {

        /* find the filename */
        int name = 1, crt = 0, i;

        if (r->msg.type != 1){
            printf("Expecting filename and size message\n");
            continue;
        }

        /* Acknowledge for the filename and size */
        charge ok;
        memset(&ok, 0, sizeof(charge));
        compcrc( r->crc.payload, CRC_LOAD_SZ, &ok.crc.crc);

        if (ok.crc.crc == r->crc.crc) {
            ok.msg.type = 1000;
            send_message((msg *)&ok);

            for (i = 0; i < r->msg.len; i++){
                if (crt >= 1400){
                    printf("Malformed message received! Bailing out");
                    return -1;
                }

                if (name){
                    if (r->pack.load[i] == '\n'){
                        name = 0;
                        filename[crt] = 0;
                        crt = 0;
                    }
                    else
                        filename[crt++] = r->pack.load[i];
                }
                else {
                    if (r->pack.load[i] == '\n'){
                        name = 0;
                        filesize[crt] = 0;
                        crt = 0;
                        break;
                    }
                    else
                        filesize[crt++] = r->pack.load[i];
                }
            }
            fs = atoi(filesize);

            /* Sender's window */
            win = r->pack.id;

            sprintf(fn,"recv_%s", filename);
            printf("Receiving file %s of size %d\n", fn, fs);
            break;
        }
        else {
            fprintf(stderr, "Handshake corrupt, or lost\nRecieving another\n");
            r = (charge *) receive_message();
        }
    }
    if (r)
        free(r);

    /* Send window size to 'sender' */
    charge win_rec;
    memset(&win_rec, 0, sizeof(charge));
    win_rec.msg.type = 2000;
    win_rec.pack.id = (unsigned int) window;    /* The actual info */
    send_message( (msg *)&win_rec );

    /* Recompute window size */
    if ( window == 0 || window > win )
        window = win;

    if (window < 10)
        window = 10;


    /* Open file to write into
     *************************
     */
    int fd = open(fn, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        perror("Failed to open file\n");
    }

    /* Process payload information to write into file
     ************************************************
     */
    unsigned int seq = 0;
    charge *buff = (charge *) calloc(window, sizeof(charge));
    charge nak, ack;
    memset(&ack, 0, sizeof(charge));
    memset(&nak, 0, sizeof(charge));

    nak.msg.type = 4;
    nak.msg.len = 2;
    ack.msg.type = 3;
    ack.msg.len = 2;
    r = NULL;

    while (fs > 0){
        if (r)
            free(r);
        printf("Left to read from sender %d\n", fs);
        r = (charge *)receive_message();
        if (!r){
            perror("Receive message");
            return -1;
        }

        if (r->msg.type != 2){
            printf("Expecting payload\n");
            return -1;
        }

        unsigned short int crc_computed;
        compcrc(r->crc.payload, CRC_LOAD_SZ, &crc_computed);

        /* If not in window, or crc fails, send NAK */
        if (r->crc.crc != crc_computed || (r->pack.id - seq) >= window) {
            send_message((msg *) &nak);
            continue;
        }

        /* Otherwise, send ACK and try to flush the buffer */
        ack.pack.id = r->pack.id;
        send_message((msg *) &ack);

        buff[r->pack.id % window] = *r;

        int i, j;
        for (j = 0, i=seq % window; j < window && buff[i].msg.type != 0; j++) {
            write(fd, buff[i].pack.load, buff[i].msg.len);
            fs -= buff[i].msg.len;
            memset(&buff[i], 0, sizeof(charge));
            seq++;
            if (i == window - 1)
                i = 0;
            else
                i++;
        }
    }
    if (r)
        free(r);

    free(buff);
    free(tabel);

    close (fd);

    return 0;
}
