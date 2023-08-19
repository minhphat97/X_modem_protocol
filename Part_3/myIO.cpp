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
// Helpers: Dr. Craig, Zavier, Thai Vi
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
//% * Your group name should be "P3_<userid1>_<userid2>" (eg. P3_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : myIO.cpp
// Version     : September, 2019
// Description : Wrapper I/O functions for ENSC-351
// Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <unistd.h>         // for read/write/close
#include <fcntl.h>          // for open/creat
#include <sys/socket.h>         // for socketpair
#include "SocketReadcond.h"
#include <mutex>
#include <condition_variable>
#include <vector>
using namespace std;
mutex GlobalMutex;

struct ASocket
{
    int DestinationThread = 0;
    int Buffer = 0;
    bool Socket = false;
    condition_variable conditionVarTcDrain;
    condition_variable conditionVarMyReadCond;
    bool Dead = false;
    mutex mutexinStruct;
};
vector<ASocket*>PairVector;
int mySocketpair( int domain, int type, int protocol, int des[2] )
{
    lock_guard<mutex>lk(GlobalMutex); // not sure
    int returnVal = socketpair(domain, type, protocol, des);
    int largerDescriptor;
    if(des[0] > des[1])
    {
        largerDescriptor = des[0];
    }
    else
    {
        largerDescriptor = des[1];
    }

    if(PairVector.size() <= largerDescriptor)
    {
        PairVector.resize(largerDescriptor + 1);
    }
    PairVector[des[0]] = new ASocket;
    PairVector[des[1]] = new ASocket;

    PairVector[des[0]]->DestinationThread = des[1];
    PairVector[des[1]]->DestinationThread = des[0];

    PairVector[des[0]]->Socket = true;
    PairVector[des[1]]->Socket = true;

    return returnVal;
}

int myOpen(const char *pathname, int flags, mode_t mode)
{
    lock_guard<mutex>lk(GlobalMutex); // not sure
    int LocationFile = open(pathname, flags, mode);

    if(LocationFile >= PairVector.size())
    {
        PairVector.resize(LocationFile + 1);
    }

    PairVector[LocationFile] = new ASocket;
    PairVector[LocationFile]->Socket = false;

    return LocationFile;
}

int myCreat(const char *pathname, mode_t mode)
{
    lock_guard<mutex>lk(GlobalMutex); // not sure
    int LocationFile = creat(pathname, mode);

    if(LocationFile >= PairVector.size())
    {
        PairVector.resize(LocationFile + 1);
    }

    PairVector[LocationFile] = new ASocket;
    PairVector[LocationFile]->Socket = false;
    return LocationFile;
}

int myReadcond(int des, void * buf, int n, int min, int time, int timeout);

ssize_t myRead( int fildes, void* buf, size_t nbyte )
{
    //lock_guard<mutex>lk(GlobalMutex);
    //int convertdata = static_cast<int>(nbyte);
    if(PairVector[fildes]->Socket == false)
    {
        return read(fildes, buf, nbyte );
    }
    else if(PairVector[fildes]->Socket == true)//call readCond
    {
        return myReadcond(fildes, buf, nbyte, 1, 0, 0);
    }
}

ssize_t myWrite( int fildes, const void* buf, size_t nbyte )
{
    lock_guard<mutex>lk(GlobalMutex);
    if(PairVector[fildes]->Socket == false)
    {
        return write(fildes, buf, nbyte );
    }
    else if(PairVector[fildes]->Socket == true)
    {
        ssize_t Update = write(fildes, buf, nbyte);
        if(Update < 0)
        {
            return Update;
        }
        PairVector[fildes]->Buffer = PairVector[fildes]->Buffer + Update;

        //notify
        PairVector[fildes]->conditionVarMyReadCond.notify_all();
        return Update;
    }
}

int myClose( int fd )
{
    //lock_guard<mutex>lk(GlobalMutex);
    if(PairVector[fd]->Socket == true)
    {
        //PairVector.erase(PairVector.begin() + fd);
        PairVector[fd]->Dead = true;
        PairVector[fd]->conditionVarMyReadCond.notify_all();
        int dstThrd = PairVector[fd]->DestinationThread;
        PairVector[dstThrd]->Dead = true;
        PairVector[dstThrd]->conditionVarTcDrain.notify_all();
        return close(fd);
    }
    return close(fd);
}

int myTcdrain(int des)
{ //is also included for purposes of the course.

    unique_lock<mutex>Lock(GlobalMutex);

    PairVector[des]->conditionVarTcDrain.wait(Lock, [des]{
        if(PairVector[des]->Dead){
            PairVector.erase(PairVector.begin()+(des-1));
            return true;
        }
        return PairVector[des]->Buffer == 0;
    });
    //notify something........
    Lock.unlock();
    return 0;
}

/* See:
 *  https://developer.blackberry.com/native/reference/core/com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html
 *
 *  */
int myReadcond(int des, void * buf, int n, int min, int time, int timeout)
{
    // lock_guard<mutex>Lock(GlobalMutex);
    if(PairVector[des]->Socket == false)
    {
        //lock_guard<mutex>Lock(GlobalMutex);
        //return wcsReadcond(des, buf, n, min, time, timeout );
        return read(des, buf, n);
    }
    else if(PairVector[des]->Socket == true)
    {
//        unique_lock<mutex>Lock1(GlobalMutex);
//        int ReadFromBuffer = wcsReadcond(des, buf, n, min, time, timeout );
//
//        PairVector[des]->Buffer = PairVector[des]->Buffer - ReadFromBuffer;
//
//        if(ReadFromBuffer >= min)
//        {
//            PairVector[des]->conditionVarTcDrain.notify_all();
//            return ReadFromBuffer;
//        }
//        else if(ReadFromBuffer < min)
//        {
//            PairVector[des]->conditionVarMyReadCond.wait(Lock1, [des]{return PairVector[des]->Buffer > 0;});
//        }
//        return wcsReadcond(des, buf, n, min, time, timeout);
        int dstThrd = PairVector[des]->DestinationThread;
        unique_lock<mutex> Lock1(PairVector[dstThrd]->mutexinStruct);
        int *count = new int (0);
        PairVector[dstThrd]->conditionVarMyReadCond.wait(Lock1, [count, des, buf, n, min, time, timeout, dstThrd]{
            if(PairVector[dstThrd]->Dead)
            {
                return true;
            }
            else
            {
                if (PairVector[dstThrd]->Buffer >= (min - *count))
                {
                    return true;
                }
                else if (PairVector[dstThrd]->Buffer > 0)
                {
                    int Update = wcsReadcond(des, buf+*count, n+*count, 0, 0, 0);
                    *count += Update;
                    PairVector[dstThrd]->Buffer -= Update;
                    PairVector[dstThrd]->conditionVarTcDrain.notify_all();
                    return false;
                }
                else
                {
                    return false;
                }
            }
        });
        int Update2 = wcsReadcond(des, buf+*count, n-*count, 0, time, timeout );
        *count = *count + Update2;
        PairVector[dstThrd] -> Buffer -= Update2;
        if(PairVector[dstThrd]->Buffer == 0)
        {
            PairVector[dstThrd]->conditionVarTcDrain.notify_all();
        }
        if(PairVector[dstThrd]->Dead)
            PairVector.erase(PairVector.begin()+(dstThrd-1));
        int returnVal = *count;
        delete count;
        return returnVal;
    }
    //lock_guard<mutex>Lock1(GlobalMutex);
    /*int ReadFromBuffer;
    if (min == 0)
    {

        ReadFromBuffer = wcsReadcond(des,buf,n,min,time,timeout);
        PairVector[des]->Buffer = PairVector[des]->Buffer - ReadFromBuffer;
        PairVector[des]->conditionVarTcDrain.notify_all();
        return ReadFromBuffer;
    }
    else if(min != 0)
    {
        //unlock before wcsReadcond

        ReadFromBuffer = wcsReadcond(des,buf,n,min,time,timeout); // n is the maximum number of bytes to read
        if(ReadFromBuffer >= min)
        {
            PairVector[des]->Buffer = PairVector[des]->Buffer - ReadFromBuffer;
            return ReadFromBuffer;
        }
        else if(ReadFromBuffer < min)
        {
            unique_lock<mutex>Lock1(GlobalMutex);
            PairVector[des]->conditionVarMyReadCond.wait(Lock1,[ReadFromBuffer,min]{return ReadFromBuffer >= min;});

        }

        ReadFromBuffer = wcsReadcond(des,buf,n,min,time,timeout);


    }*/

}


