//============================================================================
//
//% Student Name 1: Minh Phat Tran
//% Student 1 #: 301297286
//% Student 1 userid (email): phatt (phatt@sfu.ca)
//
//% Student Name 2: Ziniu Chen
//% Student 2 #: 301326615
//% Student 2 ziniuc (email): ziniuc@sfu.ca
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
//% * Your group name should be "P1_<userid1>_<userid2>" (eg. P1_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : SenderX.cc
// Version     : September 3rd, 2019
// Description : Starting point for ENSC 351 Project
// Original portions Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <iostream>
#include <stdint.h> // for uint8_t
#include <string.h> // for memset()
#include <errno.h>
#include <fcntl.h>	// for O_RDWR

#include "myIO.h"
#include "SenderX.h"

using namespace std;

SenderX::
SenderX(const char *fname, int d)
:PeerX(d, fname), bytesRd(-1), blkNum(255)
{
}

//-----------------------------------------------------------------------------

/* tries to generate a block.  Updates the
variable bytesRd with the number of bytes that were read
from the input file in order to create the block. Sets
bytesRd to 0 and does not actually generate a block if the end
of the input file had been reached when the previously generated block was
prepared or if the input file is empty (i.e. has 0 length).
*/
void SenderX::genBlk(blkT blkBuf)
{
	// ********* The next line needs to be changed ***********
    for (int i = 3; i < 131; i++)
    {
        blkBuf[i] = 26;
    }
	if (-1 == (bytesRd = myRead(transferringFileD, &blkBuf[3], CHUNK_SZ )))
		ErrorPrinter("myRead(transferringFileD, &blkBuf[0], CHUNK_SZ )", __FILE__, __LINE__, errno);
	// ********* and additional code must be written ***********

		blkBuf[0]= SOH;
		if (blkNum > 255)
		{
		    blkNum = 0;
		}
		blkBuf[1] = blkNum + 1;
		blkBuf[2] = 255 - (blkNum + 1);
		int temp = 0;
		for(int i = 3;i < 131;i++)
		{
		    temp = temp + blkBuf[i];
		}
		while(temp > 255)
		{
		    temp = temp - 256;
		}
		blkBuf[131] = temp;

    // ********* The next couple lines need to be changed ***********
	if(Crcflg == true)
	{
		uint16_t myCrc16ns;
		this->crc16ns(&myCrc16ns, &blkBuf[3]);

		unsigned short temp = myCrc16ns;

		blkBuf[132] = myCrc16ns; //low coef
		blkBuf[131] = myCrc16ns >> 8; //high coef

	}
}

void SenderX::sendFile()
{
	transferringFileD = myOpen(fileName, O_RDWR, 0);
	if(transferringFileD == -1) {
		// ********* fill in some code here to write 2 CAN characters ***********
		sendByte(24);
		sendByte(24);
		cout /* cerr */ << "Error opening input file named: " << fileName << endl;
		result = "OpenError";
	}
	else {
		cout << "Sender will send " << fileName << endl;

        // ********* re-initialize blkNum as you like ***********
        //blkNum = 0; // but first block sent will be block #1, not #0
		blkNum = 0;


		// do the protocol, and simulate a receiver that positively acknowledges every
		//	block that it receives.

		// assume 'C' or NAK received from receiver to enable sending with CRC or checksum, respectively


		blkBuf[1] = blkNum;
		genBlk(blkBuf); // prepare 1st block
		while (bytesRd)
		{
			blkNum ++; // 1st block about to be sent or previous block was ACK'd

			// ********* fill in some code here to write a block ***********

			if(Crcflg == true)
			{
				myWrite( mediumD, &blkBuf[0], 133 );
			}
			else if(Crcflg == false)
			{
				myWrite( mediumD, &blkBuf[0], 132 );
			}


			// assume sent block will be ACK'd
			genBlk(blkBuf); // prepare next block
			// assume sent block was ACK'd
		};
		// finish up the protocol, assuming the receiver behaves normally
		// ********* fill in some code here ***********

		sendByte(4);
		sendByte(4);

		//(myClose(transferringFileD));
		if (-1 == myClose(transferringFileD))
			ErrorPrinter("myClose(transferringFileD)", __FILE__, __LINE__, errno);
		result = "Done";
	}
}

