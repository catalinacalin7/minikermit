#ifndef LIB
#define LIB

typedef struct {
    int len;
    char payload[1400];
} msg;


typedef struct {
    char SOH;
    char LEN;
    char SEQ;
    char TYPE;
    char DATA[250];
    unsigned short CHECK;
    char MARK;
} m_kermit; // 257 biti

typedef struct {
    char SOH;
    char LEN;
    char SEQ;
    char TYPE;
    struct date {
    	char MAXL;
    	char TIME;
    	char NPAD;
    	char PADC;
    	char EOL;
    	char QCTL;
    	char QBIN;
    	char CHKT;
    	char REPT;
    	char CAPA;
    	char R; // 11 bytes
    } DATA;
    unsigned short CHECK;
    char MARK;
} type_S; // 18 bytes


void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);

#endif

