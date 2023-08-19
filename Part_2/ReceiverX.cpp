//============================================================================
//
//% Student Name 1: Minh Phat Tran
//% Student 1 #: 301297286
//% Student 1 userid (email): phatt (phatt@sfu.ca)
//
//% Student Name 2: Ziniu Chen
//% Student 2 #: 301326615
//% Student 2 userid (email): ziniuc (ziniuc@sfu.ca)
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: _everybody helped us/me with the assignment (list names or put 'None')__
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  ___________
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P2_<userid1>_<userid2>" (eg. P2_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : ReceiverX.cpp
// Version     : September 3rd, 2019
// Description : Starting point for ENSC 351 Project Part 2
// Original portions Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <string.h> // for memset()
#include <fcntl.h>
#include <stdint.h>
#include <iostream>
#include "myIO.h"
#include "ReceiverX.h"
#include "VNPE.h"

using namespace std;

ReceiverX::
ReceiverX(int d, const char *fname, bool useCrc)
:PeerX(d, fname, useCrc), 
NCGbyte(useCrc ? 'C' : NAK),
goodBlk(false), 
goodBlk1st(false), 
syncLoss(true),
numLastGoodBlk(0)
{
}

void ReceiverX::receiveFile()
{
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	transferringFileD = PE2(myCreat(fileName, mode), fileName);

	// ***** improve this member function *****

	// below is just an example template.  You can follow a
	// 	different structure if you want.

	// inform sender that the receiver is ready and that the
	//		sender can send the first block
	sendByte(NCGbyte);
	int rcvcount = 0; //debug
	while(PE_NOT(myRead(mediumD, rcvBlk, 1), 1), (rcvBlk[0] == SOH))
	{
		if(Crcflg == false) // This is for checksum block
		{
			    getRestBlk();
				int BlockNumberandOneCompliment = rcvBlk[1] + rcvBlk[2];
				if (BlockNumberandOneCompliment != 255)
				{
					sendByte(NAK);
				}
				else if(BlockNumberandOneCompliment == 255)
				{
					if(rcvBlk[1] != uint8_t(numLastGoodBlk + 1))
					{
						if(rcvBlk[1] == uint8_t(numLastGoodBlk))
						{
							errCnt = 0;
							sendByte(ACK);
						}
						else if(rcvBlk[1] != uint8_t(numLastGoodBlk))
						{

							can8();
							close(transferringFileD);
							return;
						}
					}
					else if(rcvBlk[1] == uint8_t(numLastGoodBlk + 1))
					{
						uint8_t Sum = 0;
						for( int ii=DATA_POS ; ii < DATA_POS+CHUNK_SZ; ii++ )
						{
							Sum += rcvBlk[ii];
						}

						if(uint8_t(Sum) == rcvBlk[131])
						{
							writeChunk();
							sendByte(ACK);
							numLastGoodBlk++;
						}
						else if(uint8_t(Sum) != rcvBlk[131])
						{
							sendByte(NAK);
							errCnt++;
							if(errCnt >= errB)
							{
								can8();
								close(transferringFileD);
								return;
							}
						}
					}
				}
			}

			else if(Crcflg == true) // This is for CRC block
			{
				getRestBlk();

				uint8_t BlockNumberandOneCompliment = rcvBlk[1] + rcvBlk[2];
				if (BlockNumberandOneCompliment != 255)
				{
					sendByte(NAK);
					PE(close(transferringFileD));
				}
				else if(BlockNumberandOneCompliment == 255)
				{

					if(rcvBlk[1] != uint8_t(numLastGoodBlk + 1)) {
						// Block is not expected

						if(rcvBlk[1] == uint8_t(numLastGoodBlk))
						{

							errCnt = 0;
							sendByte(ACK);
						}
						else if(rcvBlk[1] != uint8_t(numLastGoodBlk))
						{
							can8();
							close(transferringFileD);
							return;

						}
					} else if(rcvBlk[1] == uint8_t(numLastGoodBlk + 1)) {
						// Block

						uint8_t crc_check1;
						uint8_t crc_check2;

						uint16_t myCrc16ns;
						crc16ns(&myCrc16ns, &rcvBlk[3]);

						crc_check1 = myCrc16ns; //low coef
						crc_check2 = myCrc16ns >> 8; //high coef

						if (crc_check1 == rcvBlk[131] && crc_check2 == rcvBlk[132] )
						{
							writeChunk();
							sendByte(ACK);
							numLastGoodBlk++;
						}
						else
						{
							sendByte(NAK);
							errCnt++;
							if(errCnt >= errB)
							{

								can8();
								PE(myClose(transferringFileD));
								return;
							}
						}
					}
				}
			}
	}

	if(rcvBlk[0] == EOT)
	{
		sendByte(NAK);
		(PE_NOT(myRead(mediumD, rcvBlk, 1), 1));
		if(rcvBlk[0] == EOT)
		{
			sendByte(ACK);
			close(transferringFileD);
		}
	}
	else if (rcvBlk[0] == CAN)
	{
		(PE_NOT(myRead(mediumD, rcvBlk, 1), 1));
		if (rcvBlk[0] == CAN)
		{
			close(transferringFileD);
		}

	}



	// assume EOT was just read in the condition for the while loop
	// sendByte(NAK); // NAK the first EOT
	// PE_NOT(myRead(mediumD, rcvBlk, 1), 1); // presumably read in another EOT
	// (close(transferringFileD));
	// check if the file closed properly.  If not, result should be something other than "Done".



}

/* Only called after an SOH character has been received.
The function tries
to receive the remaining characters to form a complete
block.  The member
variable goodBlk1st will be made true if this is the first
time that the block was received in "good" condition.
*/
void ReceiverX::getRestBlk()
{
	// ********* this function must be improved ***********
	// PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CRC, REST_BLK_SZ_CRC, 0, 0), REST_BLK_SZ_CRC);

	if(Crcflg== true)
	{
		PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CRC, REST_BLK_SZ_CRC, 0, 0), REST_BLK_SZ_CRC);
	}
	else if(Crcflg== false)
	{
		PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CS, REST_BLK_SZ_CS, 0, 0), REST_BLK_SZ_CS);
	}

}

//Write chunk (data) in a received block to disk
void ReceiverX::writeChunk()
{
	PE_NOT(write(transferringFileD, &rcvBlk[DATA_POS], CHUNK_SZ), CHUNK_SZ);
}

//Send 8 CAN characters in a row to the XMODEM sender, to inform it of
//	the cancelling of a file transfer
void ReceiverX::can8()
{
	// no need to space CAN chars coming from receiver in time
    char buffer[CAN_LEN];
    memset( buffer, CAN, CAN_LEN);
    PE_NOT(myWrite(mediumD, buffer, CAN_LEN), CAN_LEN);
}
