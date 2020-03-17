#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

int main(int argc, char** argv) {
    msg r, t, *y; // t pentru retransmitere // r pentru ack si nack nou
    m_kermit  mess, temp;
    type_S ack_init,temp_init;
    int seq = 1;
    unsigned short crc, crc_primit;
    FILE *file;


    init(HOST, PORT);

    while (1) {
    	// asteptam pachetul "S" 3*5 secunde
        y = receive_message_timeout(15000);

        if (y == NULL) {
        	// intrerupem executia daca nu am primit in timpul indicat
            return -1;

        } else {  // daca am primit pachetul S
            memset(&temp_init, '\0', sizeof(temp_init));
            //copiem payloadul in pachetul auxiliar temp_init
            memcpy(&temp_init, y->payload, sizeof(temp_init));
            // verificam check(- 3 pentru nu includem check si mark)
            crc_primit = crc16_ccitt(y->payload, y->len - 3);

            if (temp_init.SEQ < (seq - 1)){
                //ignoram pachetul dat
                continue;

            // daca am primit pachetul potrivit
            } else if ((crc_primit == temp_init.CHECK) && 
            	(temp_init.SEQ == (seq - 1))){
                // trimitem ack pentru S               
                memset(&ack_init, '\0', 18);
                memset(t.payload, '\0', 1400);

                // pregatim pachetul
                ack_init.SOH = 0x01;
                ack_init.LEN = 16;
                ack_init.SEQ = seq % 64;
                seq = (seq + 2) % 64;
                ack_init.TYPE = 'Y';
                ack_init.DATA.MAXL = 250;
                ack_init.DATA.TIME = 5; //secunde
                //implicit
                ack_init.DATA.NPAD = 0x00;
                ack_init.DATA.PADC = 0x00;
                ack_init.DATA.EOL = 0x0D;
                ack_init.DATA.QCTL = 0x00;
                ack_init.DATA.QBIN = 0x00;
                ack_init.DATA.CHKT = 0x00;
                ack_init.DATA.REPT = 0x00;
                ack_init.DATA.CAPA = 0x00;
                ack_init.DATA.R = 0x00;

                //calculam check
                memset(t.payload, '\0', 1400);
                memcpy(t.payload, &ack_init, 15);
                crc = crc16_ccitt(t.payload, 4);
                ack_init.CHECK = crc;
                ack_init.MARK = 0x0D;

                memset(t.payload, '\0', 1400);
                memcpy(t.payload, &ack_init, 18);
                t.len = 18;
                // trimitem ack pentru pachetul S
                send_message(&t);
                break;

            } else { // daca secventa/crc nu e potrivit
            	// trimitem nack
                memset(&mess, '\0', 257);
                memset(t.payload, '\0', 1400);

                //pregatim pachetul
                mess.SOH = 0x01;
                mess.LEN = 255;
                mess.SEQ = seq % 64;
                seq = (seq + 2) % 64;
                mess.TYPE = 'N';
                memset(mess.DATA, '\0', 250);

                //calculam check
                memcpy(t.payload, &mess, 4);
                crc = crc16_ccitt(t.payload, 4);
                mess.CHECK = crc;
                mess.MARK = 0x0D;

                memset(t.payload, '\0', 1400);
                memcpy(t.payload, &mess, 257);
                t.len = 257;
                send_message(&t);
            }
        }
    }



    while(1) {
        int trimiteri = 1;
        memset(&y, '\0', sizeof(y));
        
        while (1) {
        	y = receive_message_timeout(5000);

        	if ((y == NULL) && (trimiteri < 4)) {
        		// daca nu am primit in 5 secunde pachetul urmator
        		// trimitem ultimul pachet, adica t
	            send_message(&t);
	            trimiteri++;

        	} else if (y != NULL) {  // daca am primit pachetul

	            trimiteri = 1;
	            memset(&temp, '\0', sizeof(temp));
	            // copiem payloadul in pachetul auxiliar temp
	            // pentru a accesa cu usurinta campurile acestuia
	            memcpy(&temp, y->payload, sizeof(temp));

	            // verificam check
	            crc_primit = crc16_ccitt(y->payload, y->len - 3);

            	if (temp.SEQ < (seq - 1)){
	                //ignoram mesajul dat
	                continue;

            	} else if ((crc_primit == temp.CHECK) && 
            		(temp.SEQ == (seq - 1))){
	                // trimitem ack pentru S             
	                memset(&mess, '\0', 257);
	                memset(r.payload, '\0', 1400);
	                mess.SOH = 0x01;
	                mess.LEN = 255;
	                mess.SEQ = seq % 64;
	                seq = (seq + 2) % 64;
	                mess.TYPE = 'Y';
	                memset(mess.DATA, '\0', 250);

	                //calculam check
	                memcpy(r.payload, &mess, 4);
	                crc = crc16_ccitt(r.payload, 4);
	                mess.CHECK = crc;
	                mess.MARK = 0x0D;

	                memset(r.payload, '\0', 1400);
	                memcpy(r.payload, &mess, 257);
	                r.len = 257;
	                
	                // daca am primit pachetul de tip F
	                if (temp.TYPE == 'F'){
	                	// cream si deschidem file cu recv_numefisier
	                	// unde numefisier il gasim in campul data
	                    char denumire[250];
	                    sprintf(denumire, "recv_%s", temp.DATA);
	                    file = fopen(denumire, "wb");

	                // daca primim pachet de tip D
	                } else if (temp.TYPE == 'D'){
	                	// scriem datele primite in fisierul creat
	                    fwrite(temp.DATA, 1, y->len - 7, file);

	                // daca primim pachetul end of file
	                } else if (temp.TYPE == 'Z'){
	                	// inchidem fisierul
	                    fclose(file);

	                // daca primim pachetul end of transmission
	                } else if (temp.TYPE == 'B'){
	                	// pur si simplu trimite nack/ack si 
	                	// inchidem executia
	                    send_message(&r);
	                    return 0;
	                }

	                // trimitem nack/ack
	                send_message(&r);

	                memset(&t, '\0', sizeof(t));
	                //memoram mesajul trimis(pentru retransmitere)
	                memcpy(&t, r.payload, sizeof(t));
	                break;

	            } else { // daca e corupere de data sau 
	            	//secventa nu e cea care trebuie

	            	// trimitem nack
	                memset(&mess, '\0', 257);
	                memset(r.payload, '\0', 1400);
	                mess.SOH = 0x01;
	                mess.LEN = 255;
	                mess.SEQ = seq % 64;
	                seq = (seq + 2) % 64;
	                mess.TYPE = 'N';
	                memset(mess.DATA, '\0', 250);

	                //calculam check
	                memcpy(t.payload, &mess, 4);
	                crc = crc16_ccitt(t.payload, 4);
	                mess.CHECK = crc;
	                mess.MARK = 0x0D;

	                memset(r.payload, '\0', 1400);
	                memcpy(r.payload, &mess, 257);
	                r.len = 257;
	                memset(&t, '\0', sizeof(t));

	                //memoram mesajul trimis(pentru retransmitere)
	                memcpy(&t, r.payload, sizeof(t));
	                send_message(&r);
	            }
        	} else if (trimiteri == 4){
        		// daca am retrimis deja de 3 ori
            	perror ("receive error");
            	return -1;
        	}
    	}
	}
    return 0;
}
