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
// File Name   : SenderX.cpp
// Version     : September 3rd, 2019
// Description : Starting point for ENSC 351 Project Part 2
// Original portions Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <iostream>
#include <stdint.h> // for uint8_t
#include <string.h> // for memset()
#include <fcntl.h>	// for O_RDONLY
#include <stdlib.h> // for exit, EXIT_FAILURE
#include <thread>
#include <chrono>

#include "myIO.h"
#include "SenderX.h"
#include "VNPE.h"

using namespace std;

SenderX::SenderX(const char *fname, int d)
:PeerX(d, fname),
 bytesRd(-1),
 firstCrcBlk(true),
 blkNum(0)  	// but first block sent will be block #1, not #0
{
}

//-----------------------------------------------------------------------------

// get rid of any characters that may have arrived from the medium.
void SenderX::dumpGlitches()
{
	const int dumpBufSz = 20;
	char buf[dumpBufSz];
	int bytesRead;
	while (dumpBufSz == (bytesRead = PE(myReadcond(mediumD, buf, dumpBufSz, 0, 0, 0))));
}

// Send the block, less the block's last byte, to the receiver.
// Returns the block's last byte.

uint8_t SenderX::sendMostBlk(blkT blkBuf)
//uint8_t SenderX::sendMostBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
	const int mostBlockSize = (this->Crcflg ? BLK_SZ_CRC : BLK_SZ_CS) - 1;
	PE_NOT(myWrite(mediumD, blkBuf, mostBlockSize), mostBlockSize);
	return *(blkBuf + mostBlockSize);
}

// Send the last byte of a block to the receiver
void
SenderX::
sendLastByte(uint8_t lastByte)
{
	PE(myTcdrain(mediumD)); // wait for previous part of block to be completely drained from the descriptor
	dumpGlitches();			// dump any received glitches

	PE_NOT(myWrite(mediumD, &lastByte, sizeof(lastByte)), sizeof(lastByte));
}

/* tries to generate a block.  Updates the
variable bytesRd with the number of bytes that were read
from the input file in order to create the block. Sets
bytesRd to 0 and does not actually generate a block if the end
of the input file had been reached when the previously generated block
was prepared or if the input file is empty (i.e. has 0 length).
*/
void SenderX::genBlk(blkT blkBuf)
//void SenderX::genBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
	//read data and store it directly at the data portion of the buffer
	bytesRd = PE(read(transferringFileD, &blkBuf[DATA_POS], CHUNK_SZ ));
	if (bytesRd>0) {
		blkBuf[0] = SOH; // can be pre-initialized for efficiency
		//block number and its complement
		uint8_t nextBlkNum = blkNum + 1;
		blkBuf[SOH_OH] = nextBlkNum;
		blkBuf[SOH_OH + 1] = ~nextBlkNum;
		if (this->Crcflg) {
			/*add padding*/
			if(bytesRd<CHUNK_SZ)
			{
				//pad ctrl-z for the last block
				uint8_t padSize = CHUNK_SZ - bytesRd;
				memset(blkBuf+DATA_POS+bytesRd, CTRL_Z, padSize);
			}

			/* calculate and add CRC in network byte order */
			crc16ns((uint16_t*)&blkBuf[PAST_CHUNK], &blkBuf[DATA_POS]);
		}
		else {
			//checksum
			blkBuf[PAST_CHUNK] = blkBuf[DATA_POS];
			for( int ii=DATA_POS + 1; ii < DATA_POS+bytesRd; ii++ )
				blkBuf[PAST_CHUNK] += blkBuf[ii];

			//padding
			if( bytesRd < CHUNK_SZ )  { // this line could be deleted
				//pad ctrl-z for the last block
				uint8_t padSize = CHUNK_SZ - bytesRd;
				memset(blkBuf+DATA_POS+bytesRd, CTRL_Z, padSize);
				blkBuf[PAST_CHUNK] += CTRL_Z * padSize;
			}
		}
	}
}

/* tries to prepare the first block.
*/
void SenderX::prep1stBlk()
{
	// **** this function will need to be modified ****
	blkNum = 0;
	genBlk(blkBufs[1]);
}

/* refit the 1st block with a checksum
*/
void
SenderX::
cs1stBlk()
{
	// **** this function will need to be modified ****
	// Using solution for project part 1, but modified with blkBufs array.
	//checksum
	blkBufs[1][PAST_CHUNK] = blkBufs[1][DATA_POS];
	for( int ii=DATA_POS + 1; ii < DATA_POS+bytesRd; ii++ )
		blkBufs[1][PAST_CHUNK] += blkBufs[1][ii];

	//padding
	if( bytesRd < CHUNK_SZ )  { // this line could be deleted
		//pad ctrl-z for the last block
		uint8_t padSize = CHUNK_SZ - bytesRd;
		memset(blkBufs[1]+DATA_POS+bytesRd, CTRL_Z, padSize);
		blkBufs[1][PAST_CHUNK] += CTRL_Z * padSize;
	}
}

/* while sending the now current block for the first time, prepare the next block if possible.
*/
void SenderX::sendBlkPrepNext()
{
	// **** this function will need to be modified ****
	blkNum ++; // 1st block about to be sent or previous block ACK'd
	uint8_t lastByte = sendMostBlk(blkBufs[1]);
	size_t blkSize; //= this->Crcflg ? BLK_SZ_CRC : BLK_SZ_CS;
	if(Crcflg)
	{blkSize = BLK_SZ_CRC;}
	else
	{blkSize = BLK_SZ_CS;}
	memcpy(&blkBufs[0], &blkBufs[1], blkSize); // Backing up the blk that's been sent.
	genBlk(blkBufs[1]); // prepare next block
	sendLastByte(lastByte);
}

// Resends the block that had been sent previously to the xmodem receiver
void SenderX::resendBlk()
{
	// resend the block including the checksum or crc16
	//  ***** You will have to write this simple function *****
	uint8_t lastByte = sendMostBlk(blkBufs[0]);
	sendLastByte(lastByte);
}

//Send CAN_LEN copies of CAN characters in a row (in pairs spaced in time) to the
//  XMODEM receiver, to inform it of the canceling of a file transfer.
//  There should be a total of (canPairs - 1) delays of
//  ((TM_2CHAR + TM_CHAR)/2 * mSECS_PER_UNIT) milliseconds
//  between the pairs of CAN characters.
void SenderX::can8()
{
	//  ***** You will have to write this simple function *****
	// use the C++11/14 standard library to generate the delays
	/* int canPairs = CAN_LEN / 2;
	for(int i = 0; i < canPairs; i++){
		sendByte(CAN);
		sendByte(CAN);
		if(i != canPairs-1){
			std::this_thread::sleep_for (std::chrono::milliseconds((TM_2CHAR + TM_CHAR)/2 * mSECS_PER_UNIT));
		}
	}
	*/


	sendByte(CAN);
	sendByte(CAN); // try usleep
	for(int i=0; i<3; i++)
	{
		this_thread::sleep_for (std::chrono::milliseconds((TM_2CHAR + TM_CHAR)/2 * mSECS_PER_UNIT));
		sendByte(CAN);
		sendByte(CAN);

	}
}

void SenderX::sendFile()
{
	char byteToReceive;
	transferringFileD = myOpen(fileName, O_RDONLY, 0);
	if(transferringFileD == -1) {
		can8();
		cout /* cerr */ << "Error opening input file named: " << fileName << endl;
		result = "OpenError";
		return;
	}

	//blkNum = 0; // but first block sent will be block #1, not #0
	Crcflg = true; prep1stBlk(); errCnt = 0;
	firstCrcBlk = true;

	// ***** modify the below code according to the protocol *****
	// below is just a starting point.  You can follow a
	// 	different structure if you want.

	// START state
	PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
	if((byteToReceive == NAK || byteToReceive == 'C') && bytesRd)
	{
		if(byteToReceive == NAK)
		{
			Crcflg = false;
			cs1stBlk();
			firstCrcBlk = false;
		}
		sendBlkPrepNext();

		// ACKNAK state


		while(true)
		{

			PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
			if((byteToReceive == ACK) && bytesRd){

				sendBlkPrepNext();
				errCnt = 0;
				firstCrcBlk = false;
			}
			else if((byteToReceive == NAK || (byteToReceive == 'C' && firstCrcBlk)) && (errCnt < errB)){

				resendBlk();
				errCnt++;
			}
			else if((byteToReceive == ACK) && !bytesRd){

				sendByte(EOT);
				errCnt = 0;
				firstCrcBlk = false;
				break;
			}
			else if(byteToReceive == CAN)
			{

				// CAN state
				PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
				if(byteToReceive == CAN){
					result = "RcvCancelled";
					PE(myClose(transferringFileD));
					return;
				}
				else {
					if(byteToReceive == NAK)
					{
						can8();
						result = "ExcessiveNAKs";
						PE(myClose(transferringFileD));
						return;
					}
					else
					{
						cerr << "Sender received totally unexpected char #" << byteToReceive << ": " << (char)byteToReceive << endl;
						PE(myClose(transferringFileD));
						exit(EXIT_FAILURE);
					}
				}
			}
			else {
				if(byteToReceive == NAK)
				{
					can8();
					result = "ExcessiveNAKs";
					PE(myClose(transferringFileD));
					return;
				}
				else
				{
					cerr << "Sender received totally unexpected char #" << byteToReceive << ": " << (char)byteToReceive << endl;
					PE(myClose(transferringFileD));
					exit(EXIT_FAILURE);
				}
			}
		}

	}
	else if((byteToReceive == NAK || byteToReceive == 'C') && !bytesRd)
	{
		if(byteToReceive == NAK)
		{
			firstCrcBlk = false;
		}
		sendByte(EOT);
	}
	else {
		if(byteToReceive == NAK)
		{
			can8();
			result = "ExcessiveNAKs";
			PE(myClose(transferringFileD));
			return;
		}
		else
		{
			cerr << "Sender received totally unexpected char #" << byteToReceive << ": " << (char)byteToReceive << endl;
			PE(myClose(transferringFileD));
			exit(EXIT_FAILURE);
		}
	}

	// EOT1 state
	PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);

	if(byteToReceive == NAK)
	{
		sendByte(EOT);

		// EOTEOT state
		PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);

		while(byteToReceive == NAK && errCnt < errB)
		{
			sendByte(EOT);
			errCnt++;
		}
		if(byteToReceive == ACK)
		{
			result = "Done";
			PE(myClose(transferringFileD));
			return;
		}
		else if(byteToReceive == 'C')
		{
			can8();
			result = "UnexpectedC";
			PE(myClose(transferringFileD));
			return;
		}
		else {
			if(byteToReceive == NAK)
			{
				can8();
				result = "ExcessiveNAKs";
				PE(myClose(transferringFileD));
				return;
			}
			else
			{
				cerr << "Sender received totally unexpected char #" << byteToReceive << ": " << (char)byteToReceive << endl;
				PE(myClose(transferringFileD));
				exit(EXIT_FAILURE);
			}
		}
	}
	else if(byteToReceive == ACK)
	{
		result = "1st EOT ACK'd";
		PE(myClose(transferringFileD));
		return;
	}
	else {
		if(byteToReceive == NAK)
		{
			can8();
			result = "ExcessiveNAKs";
			PE(myClose(transferringFileD));
			return;
		}
		else
		{
			cerr << "Sender received totally unexpected char #" << byteToReceive << ": " << (char)byteToReceive << endl;
			PE(myClose(transferringFileD));
			exit(EXIT_FAILURE);
		}
	}

	/*
	if (-1 == myClose(transferringFileD))
		VNS_ErrorPrinter("myClose(transferringFileD)", __func__, __FILE__, __LINE__, errno);
	*/
}

