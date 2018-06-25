#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <ctype.h>

#include "haraka.h"
#include "stuff.h"
#include <jansson.h>
#include <curl/curl.h>


#define SER_GETHASH (1 << 2)
static const int PROTOCOL_VERSION = 170003;

/* Global Variables */

#define MAXBUF 2048
#define FILENAME "VRSC.conf"
#define DELIM "="

struct config
{
    char rpcuser[MAXBUF];
    char rpcpassword[MAXBUF];
    int rpcport;
};

union tblocktemplate {
    unsigned char blocktemplate[1488];
    struct {
        uint32_t version;
        unsigned char prevhash[32];
        unsigned char merkleroot[32];
        unsigned char reserved[32];
        uint32_t timestamp;
        uint32_t nbits;
        unsigned char nonce[32];
        unsigned char solution[1347];

    };
};


union tblocktemplate blocktemplate;
struct config configstruct;

/* ---------------- */

char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

struct config get_config(char *filename)
{
        struct config configstruct;
        FILE *file = fopen (filename, "r");

        if (file != NULL)
        {
                char line[MAXBUF];
                int i = 0;

                while(fgets(line, sizeof(line), file) != NULL)
                {
                        char *cfline, *key;
                        cfline = strstr((char *)line,DELIM);
                        *cfline = 0;
                        key = trimwhitespace(line);
                        //printf("[1] key = %s\n",key);
                        cfline = cfline + strlen(DELIM);
                        cfline = trimwhitespace(cfline);
                        //printf("[2] value = %s\n",cfline);
                        if (strcmp(key,"rpcuser") == 0)
                            memcpy(configstruct.rpcuser,cfline,strlen(cfline));
                        if (strcmp(key,"rpcpassword") == 0)
                            memcpy(configstruct.rpcpassword,cfline,strlen(cfline));
                        if (strcmp(key,"rpcport") == 0)
                            configstruct.rpcport = atoi(cfline);
                        i++;
                }
                fclose(file);
        }
        return configstruct;

}

/*
#define AES4(s0, s1, s2, s3) \
  s0 = _mm_aesenc_si128(s0, (__m128i)(__v4si){ 0, 0, 0, 0 }); \
  s1 = _mm_aesenc_si128(s1, (__m128i)(__v4si){ 0, 0, 0, 0 }); \
  s2 = _mm_aesenc_si128(s2, (__m128i)(__v4si){ 0, 0, 0, 0 }); \
  s3 = _mm_aesenc_si128(s3, (__m128i)(__v4si){ 0, 0, 0, 0 }); \
  s0 = _mm_aesenc_si128(s0, (__m128i)(__v4si){ 0, 0, 0, 0 }); \
  s1 = _mm_aesenc_si128(s1, (__m128i)(__v4si){ 0, 0, 0, 0 }); \
  s2 = _mm_aesenc_si128(s2, (__m128i)(__v4si){ 0, 0, 0, 0 }); \
  s3 = _mm_aesenc_si128(s3, (__m128i)(__v4si){ 0, 0, 0, 0 }); \
*/

/* verus_hash.cpp -> CVerusHash::Hash(void *result, const void *data, size_t len) */
void VerusHash(void *result, const void *data, size_t len)
{
    unsigned char buf[128];
    unsigned char *bufPtr = buf;
    int pos = 0, nextOffset = 64;
    unsigned char *bufPtr2 = bufPtr + nextOffset;
    unsigned char *ptr = (unsigned char *)data;

    // put our last result or zero at beginning of buffer each time
    memset(bufPtr, 0, 32);

    // digest up to 32 bytes at a time
    for ( ; pos < len; pos += 32)
    {
        if (len - pos >= 32)
        {
            memcpy(bufPtr + 32, ptr + pos, 32);
        }
        else
        {
            int i = (int)(len - pos);
            memcpy(bufPtr + 32, ptr + pos, i);
            memset(bufPtr + 32 + i, 0, 32 - i);
        }
        haraka512(bufPtr2, bufPtr); // ( out, in)
        bufPtr2 = bufPtr;
        bufPtr += nextOffset;
        nextOffset *= -1;
    }
    memcpy(result, bufPtr, 32);
};

/* if hash < target returns 1 */
int fulltest(const unsigned char *ihash, const unsigned char *itarget)
{
	int i;
	int rc = 1;

	uint32_t *hash = (uint32_t *)ihash;
	uint32_t *target = (uint32_t *)itarget;

	for (i = 7; i >= 0; i--) {
		if (hash[i] > target[i]) {
			rc = 0;
			break;
		}
		if (hash[i] < target[i]) {
			rc = 1;
			break;
		}
	}
	return rc;
}

void dump(unsigned char buffer[],int len) {
int i,j;
unsigned char ch;

printf("\n");
for (i=0;i<len;i+=16) {
  printf("%04x: ",i);
  for (j=0;j<16;j++){
   if ((i+j) < len) printf("%02x ",buffer[i+j]&0xff);
   else printf("   ");}
  printf(" *");
  for (j=0;j<16;j++) {
   if ((i+j) < len) {
    ch=buffer[i+j];
    if ((ch < 0x20)||((ch > 0x7e)&&(ch<0xc0))) putchar('.');
    else putchar(ch);
   }
   else printf(" ");
  }
  printf("*\n");
 }
}

// *** iguana_utils.c ***

char hexbyte(int32_t c)
{
    c &= 0xf;
    if ( c < 10 )
        return('0'+c);
    else if ( c < 16 )
        return('a'+c-10);
    else return(0);
}

int32_t _unhex(char c)
{
    if ( c >= '0' && c <= '9' )
        return(c - '0');
    else if ( c >= 'a' && c <= 'f' )
        return(c - 'a' + 10);
    else if ( c >= 'A' && c <= 'F' )
        return(c - 'A' + 10);
    return(-1);
}

int32_t is_hexstr(char *str,int32_t n)
{
    int32_t i;
    if ( str == 0 || str[0] == 0 )
        return(0);
    for (i=0; str[i]!=0; i++)
    {
        if ( n > 0 && i >= n )
            break;
        if ( _unhex(str[i]) < 0 )
            break;
    }
    if ( n == 0 )
        return(i);
    return(i == n);
}

int32_t unhex(char c)
{
    int32_t hex;
    if ( (hex= _unhex(c)) < 0 )
    {
        //printf("unhex: illegal hexchar.(%c)\n",c);
    }
    return(hex);
}

unsigned char _decode_hex(char *hex) { return((unhex(hex[0])<<4) | unhex(hex[1])); }

int32_t decode_hex(unsigned char *bytes,int32_t n,char *hex)
{
    int32_t adjust,i = 0;
    //printf("decode.(%s)\n",hex);
    if ( is_hexstr(hex,n) <= 0 )
    {
        memset(bytes,0,n);
        return(n);
    }
    if ( hex[n-1] == '\n' || hex[n-1] == '\r' )
        hex[--n] = 0;
    if ( hex[n-1] == '\n' || hex[n-1] == '\r' )
        hex[--n] = 0;
    if ( n == 0 || (hex[n*2+1] == 0 && hex[n*2] != 0) )
    {
        if ( n > 0 )
        {
            bytes[0] = unhex(hex[0]);
            printf("decode_hex n.%d hex[0] (%c) -> %d hex.(%s) [n*2+1: %d] [n*2: %d %c] len.%ld\n",n,hex[0],bytes[0],hex,hex[n*2+1],hex[n*2],hex[n*2],(long)strlen(hex));
        }
        bytes++;
        hex++;
        adjust = 1;
    } else adjust = 0;
    if ( n > 0 )
    {
        for (i=0; i<n; i++)
            bytes[i] = _decode_hex(&hex[i*2]);
    }
    //bytes[i] = 0;
    return(n + adjust);
}

int32_t init_hexbytes_noT(char *hexbytes,unsigned char *message,long len)
{
    int32_t i;
    if ( len <= 0 )
    {
        hexbytes[0] = 0;
        return(1);
    }
    for (i=0; i<len; i++)
    {
        hexbytes[i*2] = hexbyte((message[i]>>4) & 0xf);
        hexbytes[i*2 + 1] = hexbyte(message[i] & 0xf);
        //printf("i.%d (%02x) [%c%c]\n",i,message[i],hexbytes[i*2],hexbytes[i*2+1]);
    }
    hexbytes[len*2] = 0;
    //printf("len.%ld\n",len*2+1);
    return((int32_t)len*2+1);
}

void reverse_hexstr(char *str)
{
    int i,n;
    char *rev;
    n = (int32_t)strlen(str);
    rev = (char *)malloc(n + 1);
    for (i=0; i<n; i+=2)
    {
        rev[n-2-i] = str[i];
        rev[n-1-i] = str[i+1];
    }
    rev[n] = 0;
    strcpy(str,rev);
    free(rev);
}

// *** iguana_utils.c ***


union tblocktemplate getblocktemplate() {

    uint32_t i;
    union tblocktemplate t;
    memset(&t, 0, sizeof(t));

    char request[256], *txt;
    snprintf(request, 256, "{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", \"method\": \"getblocktemplate\", \"params\": [%s] }", "");
    txt = daemon_request("127.0.0.1", configstruct.rpcport, configstruct.rpcuser, configstruct.rpcpassword, request);
    printf(YELLOW "Result: " RESET "%s\n", txt);

    json_t *j_root;
    json_error_t error;
    json_t *j_result, *j_previousblockhash, *j_version, *j_coinbasetxn, *j_hash, *j_data, *j_transactions;

    const char *previousblockhash;
    const char *hash;
    const char *data;

    uint32_t version;

    j_root = json_loads(txt, 0, &error);
    free(txt);

    if(!j_root)
    {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return t;
    }

    j_result = json_object_get(j_root, "result");
    if(!json_is_object(j_result)) {
        fprintf(stderr, "error: result is not an object\n");
        json_decref(j_root);
        return t;
    }

    j_previousblockhash = json_object_get(j_result, "previousblockhash");
    j_version = json_object_get(j_result, "version");

    previousblockhash = json_string_value(j_previousblockhash);
    version = json_integer_value(j_version);

    memcpy(&t.version, &version, sizeof(t.version));

    reverse_hexstr(previousblockhash);
    decode_hex(&t.prevhash, 32, previousblockhash);

    j_coinbasetxn = json_object_get(j_result, "coinbasetxn");
    j_hash = json_object_get(j_coinbasetxn, "hash");
    hash = json_string_value(j_hash);
    reverse_hexstr(hash);
    decode_hex(&t.merkleroot, 32, hash);

    j_data = json_object_get(j_coinbasetxn, "data");
    data = json_string_value(j_data);
    printf("data = %s\n", data);


    // fill solution
    t.blocktemplate[0x8c] = 0xfd;
    t.blocktemplate[0x8d] = 0x40;
    t.blocktemplate[0x8e] = 0x05;

    json_decref(j_root);

    return t;
};

int main()
{

    unsigned char block_41970[] = {


    0x04, 0x00, 0x00, 0x00, // version
    0xa5, 0x4b, 0xe4, 0xca, 0x85, 0x1c, 0xb1, 0x9b, 0x00, 0x0d, 0x5e, 0x03, 0xf3, 0x28, 0xe0, 0x2c, 0x97, 0xbd, 0xae, 0x37, 0x89, 0xe4, 0x73, 0x54, 0xda, 0xf4, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, // previous block hash
    0x61, 0x43, 0xfd, 0xcc, 0x47, 0x63, 0xd0, 0xa3, 0x8f, 0x7d, 0xc3, 0xc1, 0x2d, 0x77, 0x62, 0x53, 0x1c, 0xcd, 0x18, 0xff, 0x67, 0xbd, 0xb0, 0x7c, 0xac, 0x66, 0x1e, 0x9c, 0x30, 0x64, 0x29, 0x6e, // merkle root
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // reserved
    0x9d, 0xc8, 0x29, 0x5b, // timestamp
    0xcc, 0x45, 0x01, 0x1c, // nbits
    0xe9, 0x5b, 0x04, 0x1d, 0xe2, 0xb7, 0x68, 0x5f, 0x14, 0xd1, 0x71, 0x9c, 0x99, 0x84, 0xdb, 0xdf, 0x7e, 0x51, 0xe5, 0x7f, 0xc3, 0xea, 0xe4, 0x1d, 0x76, 0xe8, 0x5d, 0xfa, 0xaf, 0xa3, 0x00, 0x00, // nonce
    0xfd, 0x40, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // solution


    /*
    memcpy(blocktemplate.blocktemplate, block_41970, sizeof(block_41970));
    blocktemplate.blocktemplate[sizeof(block_41970)] = 0;
    */


    /* Init */
    gcurl_init();

    char userhome[MAXBUF];
    snprintf(userhome, MAXBUF, "%s/.komodo/VRSC/VRSC.conf", getenv("HOME"));
    configstruct = get_config(userhome);

    blocktemplate = getblocktemplate();
    dump((unsigned char *)&blocktemplate, sizeof(blocktemplate));

    /*
    printf("Jansson: %s\nCurl: %s\n", JANSSON_VERSION, curl_version());
    printf("\n");
    */

    /*
    printf("rpcuser: \"%s\"\n", configstruct.rpcuser);
    printf("rpcpassword: \"%s\"\n", configstruct.rpcpassword);
    printf("rpcport: %d\n", configstruct.rpcport);
    */

    //exit(1);

    // haraka512(unsigned char *out, const unsigned char *in)
    // unsigned char data[] = "test";

    unsigned char data[4 * 16];
    unsigned char *dataPtr = data;

    /*
    {
  "hash": "00000000009a75b5eb9f6ee35504c1d819e72f1e708542c2d130f0b0050ebffc",
  "confirmations": 2,
  "height": 41970,
  "version": 4,
  "merkleroot": "6e2964309c1e66ac7cb0bd67ff18cd1c5362772dc1c37d8fa3d06347ccfd4361",
  "time": 1529464989,
  "nonce": "0000a3affa5de8761de4eac37fe5517edfdb84999c71d1145f68b7e21d045be9",
  "solution": "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
  "bits": "1c0145cc",
  "difficulty": 50821084715.19256,
  "chainwork": "000000000000000000000000000000000000000000000000004c59bd60c97f44",
  "previousblockhash": "000000000050f4da5473e48937aebd972ce028f3035e0d009bb11c85cae44ba5",
  "nextblockhash": "000000000072a56b1f502058761facdd9cf1206d418ab5a33c94c90c25e2ba7d"
}
*/


    /*

    about solution encoding - https://webcache.googleusercontent.com/search?q=cache:PGr_Rd_KsEIJ:https://forum.z.cash/t/easy-to-follow-description-of-the-equihash-algorithm/12689+&cd=1&hl=ru&ct=clnk&gl=ru

    ---

    Ok, I’ll try.

Part I, the overall mining scheme since megacrypto has asked for it.

To mine you get a “work” which is first part of final block, consisting of version (4 bytes), prevhash (32), merkle root (32), reserved (32), time (4), bits (4) and nonce (32) total 140 bytes. When pool mining, you will get the first part of nonce from the pool, and only the second part you will choose by yourself. Otherwise if you sole mine then all nonce is yours.

Then you make 2**20 = 1048576 hashes, each is 50 bytes of BLAKE2b function of 140 bytes of “work” concatenated with 4 bytes of hash index. Then you split each hash in the middle to two strings of equal size, getting 2**21 = 2097152 strings of 25 bytes (200 bits) each. This requires at least 50 megabytes of memory, effectively kicking out modern FPGA and ASIC approaches to high performing mining for other coins.

Note, number 21 is equal to N / (K + 1) + 1 and number 200 is equal to N, where N = 200 and K = 9 are Equihash parameters chosen for Zcash.

Next you solve Wagner's algorithm. What is it? You must peek 2**K = 512 different strings from those 2097152, such as binary XOR of them is zero. Moreover, some other properties of your selection must be assured. First, on each split by two of your indexes the left ones must be ordered, and second, on each split by two of your indexes resulting XOR of strings must have 20*height zero bits from the left. More on algorithm is in the part II.

When you find some solution (typical nonce provide around of two solutions on average), you fill the rest of block with solution length (3 bytes in special encoding always the same value 1344) and solution itself, which is encoded by 21 bits per index, thus 21*512 bits that is exactly 1344 bytes.

There is a target, which means the difficulty barrier. You make sha256 hash of sha256 hash of your total block of 140+3+1344=1487 bytes, and then your compare your double sha256 with the target. Below the target is good, and then you can propagate your block if sole or submit your job results if you do pool mining.

Caveat of bit ordering and start of indexing everywhere.

Part II, the Wagner’s algorithm, which is the core of Zcash mining.

You will make K+1=10 steps, and in the beginning you have 2097152 original strings of 25 bytes each (200 bits). On each step you will be interested in next 20 bits from those 200 bits strings, i.e. bits from 0 to 19 on first step, 0 to 39 on second etc (counting from zero from left). On each step you try to combine pairs of strings from previous step and keep only the pairs that have common interesting bits (in other words, XOR of which is zero in interesting bits). So, on each step you will get growing binary trees of indexes in the original strings: one index on step 1, two indexes on step 2, four indexes on step 3 etc. Note that all indexes in one tree must be unique. At the final step you will have 512 unique indexes combined into desired properties of the algorithm.

There are some optimizations implemented in reference miner and more optimizations in winners of miner contest. Basically it’s about peeking correct pairs. Note that you don’t need to try each index to each, you can first sort them and then try to combine only while interested bits are the same (reference miner), or you can put strings in the buckets indexed by interesting bits (other miners), and keep those buckets compact to fit into processor caches (that’s the key of speed). Also, you can do things in parallel to use multicore CPU and GPU advantages.

    ---

    ZCash block header structure is described in https://github.com/zcash/zips/blob/master/protocol/protocol.pdf paragraph 6.3

Solution length is always 1344 bytes and this number is encoded (serialized) in so-called compactSize encoding from bitcoin.

For the value in the range [254;65535] it’s a three bytes (as intended for this field in the header) 0xfd 0x40 0x05, where the first is the marker (253 in decimal) and the second and the third are 1344 decimal in little-endian format (so 0x0540=0d1344):
https://github.com/zcash/zcash/blob/master/src/serialize.h#L254
https://en.bitcoin.it/wiki/Protocol_documentation#Variable_length_integer

    */

    unsigned char checkdata[] = { 0xc2, 0xa4, 0x00, 0x00, 0xad, 0x8a, 0x58, 0xe2 };

    unsigned char buf[128];
    int i, j;
    time_t t;

    /* https://eprint.iacr.org/2016/098.pdf

    Haraka-512 v2
    Input : 00 01 02 03 04 05 06 07 08 09 0a 0b 0 c 0 d 0 e 0 f
    10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f
    20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f
    30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f

    Output : be 7f 72 3b 4e 80 a9 98 13 b2 92 28 7f 30 6 f 62
    5a 6d 57 33 1c ae 5f 34 dd 92 77 b0 94 5 b e2 aa

    Haraka-256 v2
    Input : 00 01 02 03 04 05 06 07 08 09 0a 0b 0 c 0 d 0 e 0 f
    10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f
    Output : 80 27 cc b8 79 49 77 4b 78 d0 54 5f b7 2b f7 0 c
    69 5 c 2a 09 23 cb d4 7b ba 11 59 ef bf 2b 2c 1c

    */

    // set test vector
    for (i=0; i < 4 * 16; i++) { data[i] = i; }
    for (i=0; i < 4 * 16; i++) { printf("%02x", data[i]); } printf("\n");

    memset(buf, 0, sizeof(buf));

    //load_constants();
    //printf("Constants: ");
    //print_constants();

    haraka512(buf, dataPtr);
    for (i=0; i < 32; i++) { printf("%02x", buf[i]); } printf("\n");
    printf("be7f723b4e80a99813b292287f306f625a6d57331cae5f34dd9277b0945be2aa\n");


    //[Decker] CVerusHash::Hash data = c2a40000ad8a58e2
    //[Decker] CVerusHash::Hash result = b10536cc23e50a2165e7c1dd5ae9a0f072dc963bce52dec2c74173d7f628a837
    memset(buf, 0, sizeof(buf));
    VerusHash(buf, checkdata, sizeof(checkdata));
    for (i=0; i < 32; i++) { printf("%02x", buf[i]); } printf("\n");
    printf("b10536cc23e50a2165e7c1dd5ae9a0f072dc963bce52dec2c74173d7f628a837\n");

    printf("Blockheader Size: %ld\n", sizeof(block_41970));
    memset(buf, 0, sizeof(buf));

    struct timeval  tv1, tv2;
    gettimeofday(&tv1, NULL);

    srand((unsigned) time(&t));
    for (i=0; i<1; i++) {


        //for (j=0; j < 32; j++) block_41970[4 + 32 + 32 + 32 + 4 + 4 + j] = rand();
        VerusHash(buf, block_41970, sizeof(block_41970));
        for (j=0; j < 32; j++) { printf("%02x", buf[31-j]); } printf("\n");


    }
    gettimeofday(&tv2, NULL);
    printf ("Total time = %f seconds\n",
         (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 +
         (double) (tv2.tv_sec - tv1.tv_sec));


    for (i=0; i < 32; i++) { printf("%02x", buf[i]); } printf("\n");
    for (i=0; i < 32; i++) { printf("%02x", buf[31-i]); } printf("\n");
    printf("00000000009a75b5eb9f6ee35504c1d819e72f1e708542c2d130f0b0050ebffc\n");


    /*printf("nonce = ");

    srand((unsigned) time(&t));
    for (i=0; i < 32; i++) block_41970[4 + 32 + 32 + 32 + 4 + 4 + i] = rand();
    for (i=0; i < 32; i++) { printf("%02x", block_41970[4 + 32 + 32 + 32 + 4 + 4 + 31 - i]); } printf("\n");
    */


    /*int j;
    for (j=0; j<sizeof(block_41970); j++) {
        memset(buf, 0, sizeof(buf));
        VerusHash(buf, block_41970, j);
        printf("[%4d] ", j);
        for (i=0; i < 32; i++) { printf("%02x", buf[i]); } printf("\n");
    }
    */


    // https://en.bitcoin.it/wiki/Getblocktemplate

    return 0;
}
