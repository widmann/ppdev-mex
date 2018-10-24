/*
 * ppdev_mex.c
 *
 * Compile in MATLAB with mex ppdev_mex.c [-v]
 * For description see ppdev_mex.m
 *
 * Author: Andreas Widmann, University of Leipzig, 2012 based on ppMex.c by
 *  Erik Flister
 *
 * Copyright (C) 2012 Andreas Widmann, University of Leipzig, widmann@uni-leipzig.de
 * and 2011 Erik Flister, University of Oregon, erik.flister <at> gmail
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <mex.h>

#include <sys/io.h>
#include <string.h>
#include <errno.h>

#include <math.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define NUM_ADDRESS_COLS 2
#define NUM_DATA_COLS 3

#define ADDR_BASE "/dev/parport"

#define DEBUG false
#define ENABLE_WRITE true
#define USE_PPDEV true /* just sketched in atm */

#define DATA_OFFSET 0
#define STATUS_OFFSET 1
#define CONTROL_OFFSET 2
#define ECR_OFFSET 0x402

#define OFFSETS {DATA_OFFSET,STATUS_OFFSET,CONTROL_OFFSET,ECR_OFFSET}
#define NUM_REGISTERS 4

#define CONTROL_BIT_0 PARPORT_CONTROL_STROBE
#define CONTROL_BIT_1 PARPORT_CONTROL_AUTOFD
#define CONTROL_BIT_2 PARPORT_CONTROL_INIT
#define CONTROL_BIT_3 PARPORT_CONTROL_SELECT

#define STATUS_BIT_3 PARPORT_STATUS_ERROR
#define STATUS_BIT_4 PARPORT_STATUS_SELECT
#define STATUS_BIT_5 PARPORT_STATUS_PAPEROUT
#define STATUS_BIT_6 PARPORT_STATUS_ACK
#define STATUS_BIT_7 PARPORT_STATUS_BUSY

#define MAX_DEVS 8

typedef void (*MexFunctionPtr)(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]);

typedef struct {
  int parportfd;
  char *addr;
  bool claimed;
} pp_device;

static pp_device *ppdev_table[MAX_DEVS];

bool getBit(const unsigned char b, const unsigned char n) {
    return (true && (b & 1<<n)); /* need a bona fide bool */
}

void printBits(const unsigned char b) {
    int i;
    for (i = 7; i >= 0; i--) {
        printf("%c",getBit(b,i) ? '1' : '0');
    }
}

void ppd(const int parportfd, const int action, void * const b, const char * const msg) {
    int result;

    if (b==NULL) {
        result = ioctl(parportfd,action);
    } else {
        result = ioctl(parportfd,action,b);
    }

    if (result != 0) {
        printf("PPD ioctl %d: %d (%s)\n",action,result,strerror(errno));
        mexErrMsgTxt(msg);
    }
}

void readPort(const uint64_T reg, void * const b, const int parportfd, const int reader, const int o) {
    if (USE_PPDEV) {
        ppd(parportfd,reader,b,"couldn't read pport");
        /*         printf("%d\n",o); */
    } else {
        *(unsigned char *)b = inb(reg);
    }
}

void doPort(
        const void * const addr,
        const int parportfd,
        const unsigned char mask[NUM_REGISTERS],
        const unsigned char vals[NUM_REGISTERS],
        mxLogical * const out,
        const int n,
        const uint8_T * const data,
        const int numVals,
        const bool writing
        ) {
    static bool setup = false;

    uint64_T reg;
    unsigned char b;
    int result, i, j, reader, writer, offsets[NUM_REGISTERS] = OFFSETS; /*lame*/

    for (i = 0; i < NUM_REGISTERS; i++) {
        if (mask[i] != 0) {
            switch (offsets[i]) {
                case DATA_OFFSET:
                    reader = PPRDATA; /*need PPDATADIR set non-zero*/
                    writer = PPWDATA; /*need PPDATADIR set zero*/
                    break;
                case STATUS_OFFSET:
                    reader = PPRSTATUS;
                    break;
                case CONTROL_OFFSET:
                    reader = PPRCONTROL;
                    writer = PPWCONTROL;
                    break;
                case ECR_OFFSET:
                    if (USE_PPDEV) {
                        mexErrMsgTxt("ECR not supported under PPDEV (figure out correct PPSETMODE)");
                    }
                    break;
                default:
                    mexErrMsgTxt("bad offset");
                    break;
            }
            reg = *(uint64_T *)addr + offsets[i];
            readPort(reg,&b,parportfd,reader,offsets[i]);

            if (writing) {
                switch (offsets[i]) {
                    case STATUS_OFFSET:
                        mexErrMsgTxt("can't write to status register");
                        break;
                    case CONTROL_OFFSET:
                        for (j=4; j<=7; j++) {
                            if (getBit(mask[i],j)) {
                                mexErrMsgTxt("bad control bit for writing");
                            }
                        }
                        break;
                }
                if (DEBUG) {
                    printf("old %d:",i);
                    printBits(b);
                }
                b = (b & ~mask[i]) | vals[i]; /*frob*/
                if (DEBUG) {
                    printf(" -> ");
                    printBits(b);
                }
                if (offsets[i] != ECR_OFFSET && ENABLE_WRITE) {
                    if (USE_PPDEV) {
                        ppd(parportfd,writer,&b,"couldn't write pport");
                        /* printf("%d\n",offsets[i]); */
                    } else {
                        outb(b,reg);
                    }
                    if (out != NULL || DEBUG) {
                        readPort(reg,&b,parportfd,reader,offsets[i]);
                        
                        if (DEBUG) {
                            printf(" -> ");
                            printBits(b);
                            printf("\n");
                        }
                    }
                } else {
                    printf(" not actually writing to register, either writes disabled or ECR protection\n");
                }
            }

            if (out != NULL) {
                for (j = 0; j < numVals; j++) {
                    if (data[j+numVals] == offsets[i]) {
                        out[j+n*numVals] = getBit(b,data[j]);
                        if (DEBUG) {
                            printf("wrote a %d\n",out[j+n*numVals]);
                        }
                    }
                }
            }

        }
    }

}

void PPDEVWrite(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    int numAddresses = 1, numVals = 8, i, j, result, addrStrLen, port;
    uint8_T *data;
    mxArray *tmp;
    uint64_T val;
    mxLogical *out;
    uint8_T bitNum, regOffset, value = 0;
    unsigned char mask[NUM_REGISTERS] = { 0 }, vals[NUM_REGISTERS] = { 0 }, pos;
    bool writing = true;

    /* Check number of input arguments */
    if (nrhs != 2) {
        mexErrMsgTxt("Exactly 2 arguments required.");
    }

    /* The device number and value must be noncomplex scalar double */
    for (i = 0; i < nrhs; i++) {
/*        if (!mxIsDouble(prhs[i]) || mxIsComplex(prhs[i]) || ! mxIsScalar(prhs[i])) { */
        if (!mxIsDouble(prhs[i]) || mxIsComplex(prhs[i]) || !(mxGetN(prhs[i])==1 && mxGetM(prhs[i])==1)) {
            mexErrMsgTxt("Arguments must be noncomplex scalar double.");
        }
    }

    /* Assign pointers to each input */
    port = *mxGetPr(prhs[0]);
    if (port < 1 || port > MAX_DEVS) {
        mexErrMsgTxt("Port number out of range.");
    }
    port--;
    val = *mxGetPr(prhs[1]);
    if (val < 0 || val > 255) {
        mexErrMsgTxt("Value out of range.");
    }

    /* Convert value to data/bit matrix */
    tmp = mxCreateNumericMatrix(8, 3, mxUINT8_CLASS, mxREAL);
    data = mxGetData(tmp);
    for (i = 0; i < numVals; i++) {
        data[i] = i;
        data[i + numVals] = 0;
        data[i + 2 * numVals] = getBit(val, i);
    }

    /* Assign pointers to each output */
    if (DEBUG) printf("%d lhs\n",nlhs);
    switch (nlhs) {
        case 1:
            *plhs = mxCreateLogicalMatrix(numVals,numAddresses);
            if (*plhs == NULL) {
                mexErrMsgTxt("couldn't allocate output");
            }
            out = mxGetLogicals(*plhs);
            break;
        case 0:
            if (!writing) {
                mexErrMsgTxt("exactly 1 output argument required when reading");
            }
            out = NULL;
            break;
        default:
            mexErrMsgTxt("at most 1 output argument allowed");
            break;
    }

    if (DEBUG) printf("\n\ndata:\n");

    /* Convert data matrix for use with doPort */
    for (i = 0; i < numVals; i++) {
        bitNum    = data[i          ];
        regOffset = data[i+  numVals];
        if (writing) {
            value = data[i+2*numVals];
        }

        if (DEBUG) {
            printf("\t%d, %d", bitNum, regOffset);
            if (writing) printf(" %d", value);
            printf("\n");
        }

        if (bitNum>7 || regOffset>2 || value>1) {
            mexErrMsgTxt("bitNum must be 0-7, regOffset must be 0-2, value must be 0-1.");
        }

        pos = 1<<bitNum;
        mask[regOffset] |= pos;
        if (value) vals[regOffset] |= pos;
    }

    if (DEBUG) {
        for (j = 0; j < NUM_REGISTERS; j++) {
            printf("mask:");
            printBits(mask[j]);
            if (writing) {
                printf(" val:");
                printBits(vals[j]);
            }
            printf("\n");
        }
    }

    i = 0;
    if (ppdev_table[port] != NULL) {
        doPort(ppdev_table[port]->addr, ppdev_table[port]->parportfd, mask, vals, out, i, data, numVals, writing);
    } else {
        mexErrMsgTxt("Port not open.");
    }

}

static void PPDEVCloseAll()
{
    int i, result;

    /* Clear out device table */
    for (i = 0; i < MAX_DEVS; i++) {

        if (ppdev_table[i] != NULL) {

            /* Release port */
            if (ppdev_table[i]->claimed) {
                ppd(ppdev_table[i]->parportfd,PPRELEASE,NULL,"couldn't release pport");
            }
            ppdev_table[i]->claimed = false;

            /* Close file descriptor */
            result = close(ppdev_table[i]->parportfd);
            if (result != 0) {
                printf("close: %d (%s)\n",result,strerror(errno));
                mexErrMsgTxt("couldn't close port");
            }

            /* Free memory */
            mxFree(ppdev_table[i]->addr);
            mxFree(ppdev_table[i]);

            /* Clear out device from device table */
            ppdev_table[i] = NULL;

        }
    }

}

void PPDEVClose(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    int port, result;

    /* The device number must be noncomplex scalar double */
/*    if (nrhs != 1 || !mxIsDouble(prhs[0]) || mxIsComplex(prhs[0]) || !mxIsScalar(prhs[0])) { */
    if (nrhs != 1 || !mxIsDouble(prhs[0]) || mxIsComplex(prhs[0]) || !(mxGetN(prhs[0])==1 && mxGetM(prhs[0])==1)) {
        mexErrMsgTxt("Port number must be noncomplex scalar double.");
    }

    /* Assign pointers to each input */
    port = *mxGetPr(prhs[0]);
    if (port < 1 || port > MAX_DEVS) {
        mexErrMsgTxt("Port number out of range.");
    }
    port--;

    if (ppdev_table[port] != NULL) {

        /* Release port */
        if (ppdev_table[port]->claimed) {
            ppd(ppdev_table[port]->parportfd,PPRELEASE,NULL,"couldn't release pport");
        }
        ppdev_table[port]->claimed = false;

        /* Close file descriptor */
        result = close(ppdev_table[port]->parportfd);
        if (result != 0) {
            printf("close: %d (%s)\n",result,strerror(errno));
            mexErrMsgTxt("couldn't close port");
        }

        /* Free memory */
        mxFree(ppdev_table[port]->addr);
        mxFree(ppdev_table[port]);

        /* Clear out device from device table */
        ppdev_table[port] = NULL;

    } else {

        mexErrMsgTxt("Port not open.");

    }

}

void PPDEVOpen(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    int result, addrStrLen;
    uint64_T port;
    char *addrStr;
    pp_device *newdev = NULL;

    /* The device number must be noncomplex scalar double */
/*    if (nrhs != 1 || !mxIsDouble(prhs[0]) || mxIsComplex(prhs[0]) || !mxIsScalar(prhs[0])) { */
    if (nrhs != 1 || !mxIsDouble(prhs[0]) || mxIsComplex(prhs[0]) || !(mxGetN(prhs[0])==1 && mxGetM(prhs[0])==1)) {
        mexErrMsgTxt("Port number must be noncomplex scalar double.");
    }

    /* Assign pointers to each input */
    port = *mxGetPr(prhs[0]);
    if (port < 1 || port > MAX_DEVS) {
        mexErrMsgTxt("Port number out of range.");
    }
    port--;

    /* Convert port number to string file name */
    addrStrLen = strlen(ADDR_BASE) + (port == 0 ? 1 : 1 + floor(log10(port))); /* number digits in port */
    addrStr = (char *)mxMalloc(addrStrLen + 1);
    if (addrStr == NULL) {
        mexErrMsgTxt("couldn't allocate addrStr");
    }
    mexMakeMemoryPersistent(addrStr);

/*    result = snprintf(addrStr,addrStrLen+1,"%s%" FMT64 "u",ADDR_BASE,port); /* +1 for null terminator */
    result = snprintf(addrStr,addrStrLen+1,"%s%lu",ADDR_BASE,port); /* +1 for null terminator */
    if (result != addrStrLen) {
        printf("%d\t%d\t%s\n",result,addrStrLen,addrStr);
        mexErrMsgTxt("bad addrStr snprintf");
    }

    if (DEBUG) printf("%d\t%s.\n",addrStrLen,addrStr);

    /* Allocate memory for new port device */
    newdev = (pp_device *)mxMalloc(sizeof(pp_device));
    mexMakeMemoryPersistent(newdev);

    if (newdev != NULL) {

        mexAtExit(PPDEVCloseAll);

        newdev->addr = addrStr;

        /* Open port and get file descriptor */
        newdev->parportfd = open(newdev->addr, O_RDWR);
        if (newdev->parportfd == -1) {
            printf("%s %s\n",newdev->addr,strerror(errno));
            mexErrMsgTxt("couldn't access port");
        }

        /*bug: if the following error out, we won't close parportfd or free addrStr -- need some exceptionish error handling
         * AW: We can close port and free memory in PPDEVCLose(All). Solved?

        /* PPEXCL call succeeds, but causes following calls to fail
         * then dmesg has: parport0: cannot grant exclusive access for device ppdev0
         *                 ppdev0: failed to register device!
         *
         * web search suggests this is because lp is loaded -- implications of removing it?
         * AW: Did not notice any side effects yet
         */

        /* We want exclusive access for triggering */
        ppd(newdev->parportfd,PPEXCL,NULL,"couldn't get exclusive access to pport");

        /* Claim port and set byte mode */
        ppd(newdev->parportfd,PPCLAIM,NULL,"couldn't claim pport");
        int mode = IEEE1284_MODE_BYTE; /* or would we want COMPAT? */
        ppd(newdev->parportfd,PPSETMODE,&mode,"couldn't set byte mode");

        newdev->claimed = true;

        ppdev_table[0] = newdev;

    } else {

        mexErrMsgTxt("Could not allocate memory for new device");

    }

}

static MexFunctionPtr SelectFunction(char *command)
{
    if (strcmp(command, "Open") == 0) return &PPDEVOpen;
    if (strcmp(command, "Close") == 0) return &PPDEVClose;
    if (strcmp(command, "CloseAll") == 0) return &PPDEVCloseAll;
    /* if (strcmp(command, "Read") == 0) return &PPDEVRead; */
    if (strcmp(command, "Write") == 0) return &PPDEVWrite;

    /* Unknown command */
    return NULL;
}

static void printHelp() {
    mexPrintf("ppdev_mex usage : \n");
    mexPrintf("ppdev_mex('Open' , port)          : opens the device, and get it ready to send messages \n");
    mexPrintf("ppdev_mex('Write', port, message) : sends the message = {0, 1, 2, ..., 255} uint8 \n");
    mexPrintf("ppdev_mex('Close', port)          : closes the device \n");
    mexPrintf("ppdev_mex('CloseAll')             : closes all devices \n");
    mexPrintf("use \"help ppdev_mex\" for more details \n");
    mexPrintf("\n");
}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{

    static char firstTime = 1;
    int i, buflen;
    char *command;
    MexFunctionPtr f;

    /* Clear out device table */
    if (firstTime) {
        for (i = 0; i < MAX_DEVS; i++) {
            ppdev_table[i] = NULL;
        }
        firstTime = 0;
    }

    /* Check inputs : no input => print help then return*/
    if (nrhs==0) {
        printHelp();
        return;
    }

    /* Input must be a string. */
    if (mxIsChar(prhs[0]) != 1) {
        mexErrMsgTxt("Command/first input must be a string.");
    }

    /* Input must be a row vector. */
    if (mxGetM(prhs[0]) != 1) {
        mexErrMsgTxt("Command/first input must be a row vector.");
    }

    buflen = (mxGetM(prhs[0]) * mxGetN(prhs[0])) + 1;
    command = mxCalloc(buflen, sizeof(char));
    mxGetString(prhs[0], command, buflen);

    nrhs--;
    prhs++;

    f = SelectFunction(command);
    if (f == NULL) {
        mexErrMsgTxt("Unkown command.\n");
    }
    (*f)(nlhs, plhs, nrhs, prhs);

}
