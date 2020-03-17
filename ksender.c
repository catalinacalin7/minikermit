#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

int main(int argc, char** argv) {
    msg t;
    int seq = 0;
    type_S s_init;
    m_kermit temp, mes;
    unsigned short crc;
    int nack, trimiteri;

    init(HOST, PORT);

    // trimitem pachetul send-init de tip S

    // golim payload-ul si s_init-ul
    memset(t.payload, '\0', 1400);
    memset(&s_init, '\0', sizeof(s_init));

    //initializam toate campurile pachetului S
    s_init = (type_S){.SOH = 0x01, .LEN = 16, .TYPE = 'S', .DATA.MAXL = 250,
				.DATA.TIME = 5, .DATA.NPAD = 0x00, .DATA.PADC = 0x00,
				.DATA.EOL = 0x0D, .DATA.QCTL = 0x00, .DATA.QBIN = 0x00,
				.DATA.CHKT = 0x00, .DATA.REPT = 0x00,.DATA.CAPA = 0x00,
				.DATA.R = 0x00, .MARK = 0x0D};

    nack = 0;
    trimiteri = 1;

    // 
    while (1) {
    	if (nack == 1) {
    		// daca am primit nack, actualizam SEQ si resetam indicator nack
    		seq = (seq + 2) % 64;
    		nack = 0;
    	}
    	s_init.SEQ = seq % 64;

    	// golim payload-ul, introducem in el s_init
    	// fara campurile CHECK si MARK si calculam crc
    	memset(t.payload, '\0', 1400);
    	memcpy(t.payload, &s_init, 15);
    	crc = crc16_ccitt(t.payload, 15);
    	s_init.CHECK = crc;

    	// scriem s_init-ul in payload si trimitem receiver-ului
    	memset(t.payload, '\0', 1400);
    	memcpy(t.payload, &s_init, 18);
    	t.len = 18;
    	send_message(&t);

    	// asteptam raspuns de la receiver
    	msg *y = receive_message_timeout(5000);

    	if ((y == NULL) && (trimiteri < 4)) {
    		// daca nu-l primim, retrimitem din nou pachetul
        	send_message(&t);
        	trimiteri++;

    	} else if (y != NULL) {  // daca am primit ack sau nack
        	// copiem datele din payload in structura temp
        	trimiteri = 1;
        	memset(&temp, '\0', sizeof(temp));
        	memcpy(&temp, y->payload, sizeof(temp));
        	// daca am primit ack potrivit
        	if ((temp.TYPE == 'Y') && (temp.SEQ == (seq + 1))){
        		break;
        	} else if ((temp.TYPE == 'N') && (temp.SEQ == (seq + 1))){
        		// cand primim nack,ne ducem la urmatoare iteratie,
                //pentru retrimitere
        		nack = 1;
        	}	
    	} else if (trimiteri == 4){
    		// daca am retrimis deja de 3 ori acelasi pachet
    		perror ("receive error");
    		return -1;
    	}
    }
    
    // reactualizam seq
    seq = (seq + 2) % 64;

    // pentru fiecare fisier
    for (int i = 1; i < argc; i++) {
        printf("SENDING FILE...WAITING...\n");
        // golim payloadul si mes
    	memset(t.payload, '\0', 1400);
    	memset(&mes, '\0', sizeof(mes));

    	// trimitem numele fisierului
    	mes.SOH = 0x01;
    	mes.LEN = 255;
    	mes.TYPE = 'F';
        mes.SEQ = seq % 64;

        memset(t.payload, '\0', 1400);
        memcpy(t.payload, &mes, 254);
        crc = crc16_ccitt(t.payload, 254);
        mes.CHECK = crc;
    	memset(mes.DATA, '\0', 250);
    	sprintf(mes.DATA, "%s", argv[i]);
    	mes.MARK = 0x0D;

    	nack = 0;
    	trimiteri = 1;
        // operatia de trimitere
        while (1) {
            // daca am primit nack, actualizam SEQ si resetam indicator nack
    		if (nack == 1){
    			seq = (seq + 2) % 64;
                mes.SEQ = seq % 64;
    			nack = 0;
    		}

            // calculam check
    		memset(t.payload, '\0', 1400);
    		memcpy(t.payload, &mes, 254);
    		crc = crc16_ccitt(t.payload, 254);
    		mes.CHECK = crc;

            // scriem mesajul mes in payload si trimitem receiver-ului
    		memset(t.payload, '\0', 1400);
    		memcpy(t.payload, &mes, 257);
    		t.len = 257;
    		send_message(&t);

            // asteptam raspuns de la receiver
    		msg *y = receive_message_timeout(5000);

    		if ((y == NULL) && (trimiteri < 4)) {
                // daca nu primim nici un raspuns
                // retrimitem mesajul
        		send_message(&t);
        		trimiteri++;

    		} else if (y != NULL){  // daca am primit ack sau nack
    			trimiteri = 1;
        		memset(&temp, '\0', sizeof(temp));
        		memcpy(&temp, y->payload, sizeof(temp));

        		if ((temp.TYPE == 'Y') && (temp.SEQ == (seq + 1))){
        			break; // iesim din while

        		} else if ((temp.TYPE == 'N') && (temp.SEQ == (seq + 1))){
        			nack = 1; // nu iesim din while, retrimitem mesajul
        		}	

    		} else if (trimiteri == 4){
    			perror ("receive error");
    			return -1;
    		}
    	}
    	
        seq = (seq + 2) % 64;

        //trimitem datele

        // deschidem fisierul si calculam dimensiunea lui
        int fd = open(argv[i], O_RDONLY);
        int size = lseek(fd, 0, SEEK_END);

        // ne pozitionam la inceputul fisierului
        lseek(fd, 0, SEEK_SET);

        if (size < 250){
    		memset(t.payload, '\0', 1400);
    		memset(&mes, '\0', sizeof(mes));
    		mes.SOH = 0x01;
    		mes.LEN = 255;
    		mes.TYPE = 'D';
    		memset(mes.DATA, '\0', 250);
            // memorizam nr de caractere citite din fisier
    		int nr_caract = read(fd, mes.DATA, size);
    		mes.MARK = 0x0D;

    	    nack = 0;
    	    trimiteri = 1;
    	    while (1) {
    	        if (nack == 1){
                    seq = (seq + 2) % 64;
                    nack = 0;
                }
                mes.SEQ = seq % 64;
        		memset(t.payload, '\0', 1400);
        		memcpy(t.payload, &mes, 254);
        		crc = crc16_ccitt(t.payload, nr_caract + 4);
        		mes.CHECK = crc;

        		memset(t.payload, '\0', 1400);
        		memcpy(t.payload, &mes, 257);
                // nr_caractere continute in campul data + 
                // + inca 7 biti ai structurii
        		t.len = nr_caract + 7;
        		send_message(&t);

        		msg *y = receive_message_timeout(5000);

        		if ((y == NULL) && (trimiteri < 4)) {
                    // retrimitem mesajul daca nu am primit raspuns
                    send_message(&t);
                    trimiteri++;

        		} else if (y != NULL){  // daca am primit ack sau nack
        			trimiteri = 1;
            		memset(&temp, '\0', sizeof(temp));
            		memcpy(&temp, y->payload, sizeof(temp));

        		if ((temp.TYPE == 'Y') && (temp.SEQ == (seq + 1))){
        			break;

        		} else if ((temp.TYPE == 'N') && (temp.SEQ == (seq + 1))){
        			nack = 1;
        		}	

    		} else if (trimiteri == 4){
                // daca am retrimis deja de 3 ori
    			perror ("receive error");
    			return -1;
    		}
    	}
    	seq = (seq + 2) % 64;



        } else { // daca size e mai mare de 250

        	// calculam numarul de pachete de 250 biti care
        	// pot fi transmise
        	int full_pack = size / 250;
            // si trimitem aceste pachete
        	for (int i = 0; i < full_pack; i++){
        		memset(&mes, '\0', sizeof(mes));
        		memset(t.payload, '\0', 1400);
        		mes.SOH = 0x01;
        		mes.LEN = 255;
        		mes.TYPE = 'D';
        		memset(mes.DATA, '\0', 250);
        		int nr_caract = read(fd, mes.DATA, 250);
        		mes.MARK = 0x0D;
                // reactualizam size-ul
        		size -= 250;

                // operatia de citire
        		while (1) {
            		if (nack == 1){
                        // daca am primit nack
                        // actualizam secventa
            			seq = (seq + 2) % 64;
            			nack = 0;
            		}

                    // calculam crc
            		mes.SEQ = seq % 64;
            		memset(t.payload, '\0', 1400);
            		memcpy(t.payload, &mes, 254);
            		crc = crc16_ccitt(t.payload, nr_caract + 4);
            		mes.CHECK = crc;

            		memset(t.payload, '\0', 1400);
            		memcpy(t.payload, &mes, 257);
            		t.len = nr_caract + 7;
            		send_message(&t);

            		msg *y = receive_message_timeout(5000);

            		if ((y == NULL) && (trimiteri < 4)) {
                        // retrimitem pachetul de maxim 3 ori
                		send_message(&t);
                		trimiteri++;

            		} else if (y != NULL) {  // daca am primit ack sau nack
                		trimiteri = 1;
                		memset(&temp, '\0', sizeof(temp));
                		memcpy(&temp, y->payload, sizeof(temp));
                		if ((temp.TYPE == 'Y') && (temp.SEQ == (seq + 1))){
                			break;
                		} else if ((temp.TYPE == 'N') && 
                            (temp.SEQ == (seq + 1))){
                			nack = 1;
            		    }	
                    } else if (trimiteri == 4){
        			    perror ("receive error");
        			    return -1;
                    }
    	       }

                seq = (seq + 2) % 64;
    	    }

            // dupa ce am trimis toate pachetele cate 250 biti
            // trimitem pachetul cu bitii care au ramas in fisier
        	if (size > 0) {
    			memset(&mes, '\0', sizeof(mes));
        		memset(t.payload, '\0', 1400);
        		mes.SOH = 0x01;
        		mes.LEN = 255;
        		mes.TYPE = 'D';
        		memset(mes.DATA, '\0', 250);
                //citim doar size caractere
                int nr_caract = read(fd, mes.DATA, size);
        		mes.MARK = 0x0D;

                // trimitem pachetul
        		while (1) {
            		if (nack == 1){
            			seq = (seq + 2) % 64;
            			nack = 0;
            		}
            		mes.SEQ = seq % 64;
            		memset(t.payload, '\0', 1400);
            		memcpy(t.payload, &mes, 254);
            		crc = crc16_ccitt(t.payload, nr_caract + 4);
            		mes.CHECK = crc;

            		memset(t.payload, '\0', 1400);
            		memcpy(t.payload, &mes, 257);
                    // nr de caractere citite + 7 biti din structura
            		t.len = nr_caract + 7;
            		send_message(&t);
            		msg *y = receive_message_timeout(5000);
            		if ((y == NULL) && (trimiteri < 4)) {
                        // retrimitem de maxim 3 ori fisierul
                		send_message(&t);
                		trimiteri++;
            		} else if (y != NULL){  // daca am primit ack sau nack
                		trimiteri = 1;
                		memset(&temp, '\0', sizeof(temp));
                		memcpy(&temp, y->payload, sizeof(temp));
                		if ((temp.TYPE == 'Y') && (temp.SEQ == (seq + 1))){
                			break;
                		} else if ((temp.TYPE == 'N') && 
                            (temp.SEQ == (seq + 1))){
                			nack = 1;
                		}	
            		} else if (trimiteri == 4){
            			perror ("receive error");
            			return -1;
            		}
        	   }
        	   seq = (seq + 2) % 64;
        	}
        }



        // inchidem fisierul
    	close(fd);

    	// trimitem end of file
    	memset(&mes, '\0', sizeof(mes));
    	memset(t.payload, '\0', 1400);
    	mes.SOH = 0x01;
    	mes.LEN = 255;
    	mes.TYPE = 'Z';
    	memset(mes.DATA, '\0', 250);
    	//calculam check

    	while (1) {
    		if (nack == 1){
    			seq = (seq + 2) % 64;
    			nack = 0;
    		}
    		mes.SEQ = seq % 64;

            // calculam crc din nou pentru ca s-a actualizat seq
    		memset(t.payload, '\0', 1400);
    		memcpy(t.payload, &mes, 254);
    		crc = crc16_ccitt(t.payload, 254);
    		mes.CHECK = crc;

            // pregatim mesajul si il trimitem
    		memset(t.payload, '\0', 1400);
    		memcpy(t.payload, &mes, 257);
    		t.len = 257;
    		send_message(&t);

            // asteptam ack sau nack
    		msg *y = receive_message_timeout(5000);

    		if ((y == NULL) && (trimiteri < 4)) {
                // retrimitem maxim de 3 ori
        		send_message(&t);
        		trimiteri++;
    		} else if (y != NULL){  // daca am primit ack sau nack
        		trimiteri = 1;
        		memset(&temp, '\0', sizeof(temp));
        		memcpy(&temp, y->payload, sizeof(temp));
        		if ((temp.TYPE == 'Y') && (temp.SEQ == (seq + 1))){
        			printf("END OF FILE\n");
        			break;
        		} else if ((temp.TYPE == 'N') && (temp.SEQ == (seq + 1))){
        			nack = 1;
        		}	
    		} else if (trimiteri == 4){
    			perror ("receive error");
    			return -1;
    		}
    	}
    	seq = (seq + 2) % 64;


    }  // end for pentru argc


    // trimitem end of transmision

    memset(t.payload, '\0', 1400);
    memset(&mes, '\0', sizeof(mes));
	mes.SOH = 0x01;
	mes.LEN = 255;
	mes.TYPE = 'B';
	memset(mes.DATA, '\0', 250);
	mes.MARK = 0x0D;

	while (1) {
    	if (nack == 1){
            // daca am primit nack, reactualizam seq
    		seq = (seq + 2) % 64;
    		nack = 0;
    	}

        // calculam crc
    	mes.SEQ = seq % 64;
    	memset(t.payload, '\0', 1400);
    	memcpy(t.payload, &mes, 4);
    	crc = crc16_ccitt(t.payload, 254);
    	mes.CHECK = crc;

    	memset(t.payload, '\0', 1400);
    	memcpy(t.payload, &mes, 257);
    	t.len = 257;
    	send_message(&t);

    	msg *y = receive_message_timeout(5000);

    	if ((y == NULL) && (trimiteri < 4)) {
        	send_message(&t);
        	trimiteri++;

    	} else if (y != NULL){  // daca am primit ack sau nack
        	trimiteri = 1;
        	memset(&temp, '\0', sizeof(temp));
        	memcpy(&temp, y->payload, sizeof(temp));
        	if ((temp.TYPE == 'Y') && (temp.SEQ == (seq + 1))){
        		printf("END OF TRANSMISSION \n");
        		break;
        	} else if ((temp.TYPE == 'N') && (temp.SEQ == (seq + 1))){
        		nack = 1;
        	}	
    	} else if (trimiteri == 4){
    		perror ("receive error");
    		return -1;
    	}
    }

    return 0;
}
