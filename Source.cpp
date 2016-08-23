#include "WyattSock.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <queue>
#include <time.h>
#include <stdlib.h>

#include <Windows.h> /*kill me*/
double get_wall_time(){
	LARGE_INTEGER time, freq;
	if (!QueryPerformanceFrequency(&freq)){
		//  Handle error
		return 0;
	}
	if (!QueryPerformanceCounter(&time)){
		//  Handle error
		return 0;
	}
	return (double)time.QuadPart / freq.QuadPart;
}

//turn to zero before sending to george
#define ISSERVER 1
//#define IP "71.197.252.73"
#define IP "24.17.179.121"
#define PORT 1490
#define MAXPACKET 548
//4Mbps should be good
#define MAXUPLOADSPEED 1000000
//number of packet to save before writing to disk
// 1 million = about 0.5 gigabytes
#define NUMPACKETS 1000000

void SendFile(char *filename)
{
	//setup network stuff
	sockaddr_in addr;
	sockaddr_in server;
	SOCKET sock;
	sock = CreateSocket(IPPROTO_UDP);
	if (ISSERVER)
	{
		CreateAddress(0, PORT, &server);
		if (Bind(sock, &server) == -1)
		{
			std::cout << "Bind Fail" << std::endl;
		}
	}
	else
	{
		CreateAddress(IP, PORT, &addr);
		if (Bind(sock, &addr) == -1)
		{
			std::cout << "Bind Fail" << std::endl;
		}
	}
	//connect the two programs
	if (ISSERVER)
	{
		char temp[666];
		Receive(sock, temp, 666, &addr);
		Send(sock, filename, strlen(filename) + 1, &addr);
	}
	else
	{
		char temp[666];
		//send smething to the server so that it knows who you are
		Send(sock, filename, strlen(filename) + 1, &addr);
	}
	//so that we can check if the reciever missed a pcket
	SetNonBlocking(sock);
	std::ifstream file(filename, std::ifstream::binary);
	if (!file.good())
	{
		std::cout << "File not good name = " << filename << std::endl;
	}
	char *array = new char[(MAXPACKET - sizeof(unsigned)) * NUMPACKETS];
	bool StartNewArray = true;
	//this is a statistic for how many packets were rerequested
	unsigned Dropped = 0;
	double sendtime = get_wall_time();
	std::cout << "Wall time is " << sendtime << std::endl;
	double SecondsPerPacket = MAXPACKET / (double)(MAXUPLOADSPEED);
	while (!file.eof())
	{
		std::cout << "Reading from file" << std::endl;
		//read in an array worth from the file
		file.read(array, (MAXPACKET - sizeof(int)) * NUMPACKETS);
		StartNewArray = false;
		//the number of charcters actually read in
		unsigned n = file.gcount();
		std::cout << "Read " << n << " bytes from file" << std::endl;
		bool done = false;
		int LastPacketIndex = 0;
		int LastPacketSize = 0;
		for (int i = 0; i < NUMPACKETS && !done; ++i)
		{
			char buffer[MAXPACKET];
			*reinterpret_cast<unsigned*>(buffer) = i;
			unsigned PacketSize = MAXPACKET - sizeof(int);
			//there are not enough bytes to fill up a full packet
			if (PacketSize > n)
			{
				PacketSize = n;
				LastPacketIndex = i;
				LastPacketSize = n + sizeof(unsigned);
				done = true;
			}
			for (int j = 0; j < PacketSize; ++j)
			{
				buffer[j + sizeof(int)] = array[i * (MAXPACKET - sizeof(int)) + j];
			}
			//wait until we have the preffered packets per second
			while (get_wall_time() - sendtime < SecondsPerPacket);
			sendtime = get_wall_time();
			int e = SOCKET_ERROR;
			//if it was unable to send keep sending until it can
			while (e == SOCKET_ERROR)
			{
				e = Send(sock, buffer, PacketSize + sizeof(int), &addr);
			}
			//std::cout << "Sent " << e << " bytes" << std::endl;
			//decrease n
			unsigned temp = n;
			n -= (e - sizeof(unsigned));
			if (n == 0 && !done)
			{
				LastPacketIndex = i;
				LastPacketSize = MAXPACKET;
				done = true;
			}
			//check if the client has request a resend for any of the packets
			int m = Receive(sock, buffer, MAXPACKET);
			if (m == 3)
			{
				StartNewArray = true;
			}
			else if (m != -1)
			{
				//using buffer so need a new array
				char buf[MAXPACKET];
				unsigned *PacketNumbers = reinterpret_cast <unsigned*>(buffer);
				//minus one becuase the size is including the size variable
				Dropped += PacketNumbers[0] - 1;
				for (unsigned k = 1; k < PacketNumbers[0]; ++k)
				{
					*reinterpret_cast<unsigned*>(buf) = PacketNumbers[k];
					for (int j = 0; j < MAXPACKET - sizeof(int); ++j)
					{
						buf[j + sizeof(unsigned)] = array[PacketNumbers[k] * (MAXPACKET - sizeof(int)) + j];
					}
					//wait upload speed
					while (get_wall_time() - sendtime < SecondsPerPacket);
					sendtime = get_wall_time();
					int er = SOCKET_ERROR;
					while (er == SOCKET_ERROR)
					{
						er = Send(sock, buf, MAXPACKET, &addr);
					}
				}
			}
		}
		std::cout << "Waiting to Start New Array" << std::endl;
		double start_time = get_wall_time();
		double start_time2 = start_time;
		//make sure the receiver is all caught up before moving on
		while (StartNewArray == false)
		{
			char buffer[MAXPACKET];
			char buf[MAXPACKET];
			int n = Receive(sock, buffer, MAXPACKET, &addr);
			if (n == 3)
			{
				StartNewArray = true;
			}
			else if (n != -1)
			{
				//reset start time each packet that is recieved
				start_time = get_wall_time();
				start_time2 = start_time;
				unsigned *PacketNumbers = reinterpret_cast <unsigned*>(buffer);
				//minus one becuase the size is including the size variable
				Dropped += PacketNumbers[0] - 1;
				for (unsigned k = 1; k < PacketNumbers[0]; ++k)
				{
					*reinterpret_cast<unsigned*>(buf) = PacketNumbers[k];
					for (int j = 0; j < MAXPACKET - sizeof(int); ++j)
					{
						buf[j + sizeof(unsigned)] = array[PacketNumbers[k] * (MAXPACKET - sizeof(int)) + j];
					}
					//wait upload speed
					while (get_wall_time() - sendtime < SecondsPerPacket);
					sendtime = get_wall_time();
					int er = SOCKET_ERROR;
					while (er == SOCKET_ERROR)
					{
						er = Send(sock, buf, MAXPACKET, &addr);
					}
				}
			}
			else
			{
				//after 15 seconds of no packets we will assume that the packet was dropped
				if (get_wall_time() - start_time > 30)
				{
					std::cout << "Oh well fuck it" << std::endl;
					break;
				}
				//every one second send the last packet again
				if (get_wall_time() - start_time2 > 0.5)
				{
					*reinterpret_cast<unsigned*>(buffer) = LastPacketIndex;
					//maybe the last packet was dropped lets resend it
					for (int j = 0; j < LastPacketSize - sizeof(int); ++j)
					{
						buffer[j + sizeof(int)] = array[LastPacketIndex * (MAXPACKET - sizeof(int)) + j];
					}
					int e = Send(sock, buffer, LastPacketSize, &addr);
					//if it was unable to send keep sending until it can
					while (e == SOCKET_ERROR)
					{
						e = Send(sock, buffer, LastPacketSize, &addr);
					}
					start_time2 = get_wall_time();
				}
			}
		}
	}
	std::cout << "file sent" << std::endl;
	std::cout << "Dropped = " << Dropped << std::endl;
	delete[] array;
}

void RecieveFile()
{
	//setup network stuff
	sockaddr_in addr;
	sockaddr_in server;
	SOCKET sock;
	sock = CreateSocket(IPPROTO_UDP);
	char filename[MAXPACKET] = { 0 };
	if (ISSERVER)
	{
		CreateAddress(0, PORT, &server);
		if (Bind(sock, &server) == -1)
		{
			std::cout << "Bind Fail" << std::endl;
		}
	}
	else
	{
		CreateAddress(IP, PORT, &addr);
		if (Bind(sock, &addr) == -1)
		{
			std::cout << "Bind Fail" << std::endl;
		}
	}
	//connect the two programs
	if (ISSERVER)
	{
		//read in the file name when learning the clients addr
		Receive(sock, filename, MAXPACKET, &addr);
	}
	else
	{
		char temp[666];
		//send pointless stuff to server so it can learn my address
		Send(sock, temp, 21, &addr);
		//then read the filename that the server sends me
		Receive(sock, filename, MAXPACKET, &addr);
	}
	std::cout << "Connection Established" << std::endl;
	//The socket does not have to be nonblocking this time.
	//holds 256 packets twice (2565 is the max unsigned char)
	char *array = new char[(MAXPACKET - sizeof(unsigned)) * NUMPACKETS];
	// 0 means that that index has yet to be sent
	char unsent[NUMPACKETS] = {0};

	//548/4 = 137, but this is "better"
	unsigned Resend[MAXPACKET / sizeof(int)];
	//this is how many packets are in Resend
	//starts as one becuase the first value in Resend is the number of packets
	int size = 1;

	std::ofstream file(filename, std::ofstream::binary);

	unsigned Dropped = 0;
	double SecondsPerPacket = MAXPACKET / (double)(MAXUPLOADSPEED);
	double sendtime = get_wall_time();
	//this is the index of the last packet that was got
	unsigned LatestPackect = 0;
	while (1)
	{
		char buffer[MAXPACKET];
		int n = Receive(sock, buffer, MAXPACKET, &addr);
		unsigned index = *reinterpret_cast<unsigned*>(buffer);
		for (int i = 0; i < n - sizeof(int); ++i)
		{
			array[index * (MAXPACKET - sizeof(unsigned)) + i] = buffer[i + sizeof(int)];
		}
		//mark that we have got this packet
		unsent[index] = 1;
		if (index > LatestPackect)
		{
			//send request for the packets inbetween these that were not recieved
			//Chnage here to add these to a buffer then send the buffer once its full
			//buffer = (number of packets)(packetnumber)(packetnumber)...
			//this will hold 136 packet numbers and the number of packets
			for (unsigned i = 0; i < index - LatestPackect; ++i)
			{
				Resend[size] = i;
				size += 1;
				//if its full send the packet
				if (size == MAXPACKET / sizeof(int))
				{
					Resend[0] = size;
					//limit the upload speed
					while (get_wall_time() - sendtime < SecondsPerPacket);
					sendtime = get_wall_time();
					int er = SOCKET_ERROR;
					while (er == SOCKET_ERROR)
					{
						er = Send(sock, reinterpret_cast<char*>(Resend), size * sizeof(int), &addr);
					}
					size = 1;
				}
				Dropped += 1;
			}
			LatestPackect = index;
		}
		if (n != MAXPACKET || (index + 1) == NUMPACKETS)
		{
			std::cout << "Last Packet Got" << std::endl;
			//this is the last packet for this array
			//check for missing packets and request them
			//change that i want to make!!
			//Instead of sending a lot of small packets send a couple bigger packets
			//also I neeed to look at what packets are normally dropped because i think
			//they drop at the same time so we can just send a range of packets
			unsigned DroppedPackets = 0;
			size = 1;
			for (unsigned i = 0; i < index; ++i)
			{
				if (unsent[i] == 0)
				{
					Resend[size] = i;
					size += 1;
					DroppedPackets += 1;
					Dropped += 1;
					if (size == MAXPACKET / sizeof(int))
					{
						//limit the upload speed
						while (get_wall_time() - sendtime < SecondsPerPacket);
						sendtime = get_wall_time();
						Resend[0] = size;
						int er = SOCKET_ERROR;
						while (er == SOCKET_ERROR)
						{
							er = Send(sock, reinterpret_cast<char*>(Resend), size * sizeof(int), &addr);
						}
						size = 1;
					}
				}
			}
			//send whats left in Resend
			//limit the upload speed
			while (get_wall_time() - sendtime < SecondsPerPacket);
			sendtime = get_wall_time();
			Resend[0] = size;
			int er = SOCKET_ERROR;
			while (er == SOCKET_ERROR)
			{
				er = Send(sock, reinterpret_cast<char*>(Resend), size * sizeof(int), &addr);
			}
			size = 1;
			std::cout << "Still need " << DroppedPackets << " Packets out of " << index << std::endl;
			//set nonblocking just for this part
			SetNonBlocking(sock, 1);
			double start_time = get_wall_time();
			//at this part im just gonna hope that no packets are dropped for now
			while (DroppedPackets > 0)
			{
				int m = Receive(sock, buffer, MAXPACKET, &addr);
				if (m == -1)
				{
					//try sending them again after one second
					if (get_wall_time() - start_time > 1)
					{
						std::cout << "Asking for missing data, waiting on  " << DroppedPackets << " Pacakets"<< std::endl;
						DroppedPackets = 0;
						for (unsigned i = 0; i < index; ++i)
						{
							if (unsent[i] == 0)
							{
								Resend[size] = i;
								size += 1;
								DroppedPackets += 1;
								Dropped += 1;
								if (size == MAXPACKET / sizeof(int))
								{
									//limit the upload speed
									while (get_wall_time() - sendtime < SecondsPerPacket);
									sendtime = get_wall_time();
									Resend[0] = size;
									int er = SOCKET_ERROR;
									while (er == SOCKET_ERROR)
									{
										er = Send(sock, reinterpret_cast<char*>(Resend), size * sizeof(int), &addr);
									}
									size = 1;
								}
							}
						}
						//send whats left in Resend
						//limit the upload speed
						while (get_wall_time() - sendtime < SecondsPerPacket);
						sendtime = get_wall_time();
						Resend[0] = size;
						int er = SOCKET_ERROR;
						while (er == SOCKET_ERROR)
						{
							er = Send(sock, reinterpret_cast<char*>(Resend), size * sizeof(int), &addr);
						}
						size = 1;
						//reset start time
						start_time = get_wall_time();
					}
					continue;
				}
				unsigned index = *reinterpret_cast<unsigned*>(buffer);
				for (int i = 0; i < m - sizeof(unsigned); ++i)
				{
					array[index * (MAXPACKET - sizeof(unsigned)) + i] = buffer[i + sizeof(unsigned)];
				}
				//if this is a new packet then decrease dropped packets
				if (unsent[index] == 0)
				{
					DroppedPackets -= 1;
				}
				//mark that we have got this packet
				unsent[index] = 1;
			}
			//unset non blocking
			SetNonBlocking(sock, 0);
			std::cout << "Writing to file" << std::endl;
			//now lets write what we got so far to the file
			file.write(array, index * (MAXPACKET - sizeof(unsigned)) + (n - sizeof(unsigned)));
			//whats in the message does not matter it just has to be 3 long
			Send(sock, buffer, 3, &addr);
			if (n != MAXPACKET)
			{
				//we done 
				break;
			}
			//now reset everything to do it again and send the ready signal
			for (unsigned i = 0; i < NUMPACKETS; ++i)
			{
				unsent[i] = 0;
			}
			LatestPackect = 0;
		}
	}
	std::cout << "file got" << std::endl;
	std::cout << "Dropped = " << Dropped << std::endl;
	delete[] array;
}

int main(int argc, char **argv)
{
	srand(get_wall_time());
	Init();
	if (argc == 1)
	{
		//we are recieving the file
		RecieveFile();
	}
	else if (argc == 2)
	{
		//we are the sender
		SendFile(argv[1]);
	}
	Deinit();
	return 69;
}