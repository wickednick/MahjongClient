
#include "kbhit.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <map> 

#include <thread>
#include <vector>
#include <list>
#include <string>
#include <sstream>
#include <fcntl.h> 
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <mutex>
#include <condition_variable>

#include <chrono>

#include <wiringPi.h>
#include <wiringSerial.h>
#include <fstream>
#include <iostream>

#define BUF_SIZE	512
#define MAX_BUF		32
#define PORT		9000
#define FD_MAX_SIZE 1024

enum DIRECTION
{
	EAST = 0,
	SOUTH,
	WEST,
	NORTH,
	NUM_OF_DIRECTION,
};


int g_fdClient = -1;
int g_fdSerial = -1;
int g_direction = EAST;
bool g_bEndProgram = false;

std::map <DIRECTION, unsigned int> g_mapScore;
std::thread g_readThread;

const char* g_serverIp = "192.168.0.129";
const int g_serverPort = 9000;
const char g_usbDevice[] = "/dev/ttyUSB0";

void InitializeData()
{
	g_mapScore[EAST] = 1;
	g_mapScore[NORTH] = 2;
	g_mapScore[WEST] = 3;
	g_mapScore[NORTH] = 4;
}

void SendDirection()
{
	char szData[6];

	sprintf(szData, "--%d--", g_direction);
	szData[0] = 0x02;
	szData[1] = 0x12;
	szData[3] = 0x00;
	szData[4] = 0x03;

	write(g_fdClient, szData, 5);
}

void ReadThread()
{
	std::cout << "Start ReadThread" << "\n";

	fd_set fdsRead, fdsAll;

	FD_ZERO(&fdsRead);
	FD_SET(g_fdClient, &fdsRead);
	struct timeval tvWait;

	tvWait.tv_sec = 1;
	tvWait.tv_usec = 0;

	while (true)
	{
		if (g_bEndProgram)
			break;

		fdsAll = fdsRead;
		if (select(int(g_fdClient + 1), &fdsAll, NULL, NULL, &tvWait) == 0)
		{
			continue;
		}

		if (FD_ISSET(g_fdClient, &fdsAll))
		{
			char szTemp[1025] = {0,};
			int nReadLen = (int)read(g_fdClient, szTemp, 1024);

			if (nReadLen <= 0)
			{
				close(g_fdClient);
			}
			else
			{
				std::cout << "Recv Data" << "\n";
				if (g_fdSerial != -1)
				{
					for (int i = 0; i < nReadLen; i++)
					{
						serialPutchar(g_fdSerial, szTemp[i]);
					}
				}
			}
		}
	}

	std::cout << "End ReadThread" << "\n";
}


bool ConnectServer()
{
	g_fdClient = socket(AF_INET, SOCK_STREAM, 0);
	if (g_fdClient == -1)
	{
		std::cout << "Client : Can't open stream socket.\n";
		return false;
	}

	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(g_serverPort);
	int convResult = inet_pton(AF_INET, g_serverIp, &hint.sin_addr);
	if (convResult != 1)
	{
		std::cout << "Client : Can't bind local address.\n";
		return false;
	}

	int connResult = connect(g_fdClient, reinterpret_cast<sockaddr*>(&hint), sizeof(hint));
	if (connResult == -1)
	{
		std::cout << "Client : Can't connect to server.\n";
		close(g_fdClient);
		return false;
	}

	g_readThread = std::thread(&ReadThread);

	std::this_thread::sleep_for(std::chrono::seconds(2));
	SendDirection();

	return true;
}

bool ConnectSerial()
{
	//get filedescriptor
	if ((g_fdSerial = serialOpen(g_usbDevice, 9600)) < 0)
	{
		std::cout << "Unable to open serial device\n";
		return false;
	}

	return true;
}

void ReadDirection()
{
	char data;
	std::ifstream file("direction.cfg");
	data = (char)file.get();
	file.close();

	g_direction = atoi(&data);
	std::cout << "Current direction" << g_direction;
}

int main()
{
	ReadDirection();

	ConnectSerial();


	if (!ConnectServer())
	{
		return -1;
	}

	keyboard kb;

	bool bLoop = true;

	while (bLoop)
	{
		if (kb.kbhit())
		{
			char cInput = kb.getch();

			if (cInput == 'x' || cInput == 'X')
			{
				bLoop = false;
				break;
			}
		}

		if (g_fdSerial != -1)
		{
#ifndef NDEBUG
			if (serialDataAvail(g_fdSerial))
				std::cout << (char)serialGetchar(g_fdSerial);
#endif
		}
		else
		{
			std::cout << "reconnect serial wait 10sec\n";
			std::this_thread::sleep_for(std::chrono::seconds(10));
			ConnectSerial();
		}
	}

	g_bEndProgram = true;
	
	g_readThread.join();
	close(g_fdClient);

	printf("finish\r\n");

	/*
	printf(("Start Thread\n"));
	std::thread threadServer(&ServerThread, fdServer);

	keyboard kb;

	bool bLoop = true;

	g_mapScore[EAST] = 10000;
	g_mapScore[SOUTH] = 20000;
	g_mapScore[WEST] = 30000;
	g_mapScore[NORTH] = 40000;

	SendScore();

	std::list<int> listAvg;
	while (bLoop)
	{
		if (kb.kbhit())
		{
			char cInput = kb.getch();

			if (cInput == 'x' || cInput == 'X')
			{
				bLoop = false;
				break;
			}
		}

		bool bChange = false;

		unsigned int nNewEast = g_mapScore[EAST] + 1;
		unsigned int nNewSouth = g_mapScore[SOUTH] + 1;
		unsigned int nNewWest = g_mapScore[WEST] + 1;
		unsigned int nNewNorth = g_mapScore[NORTH] + 1;


		if (g_mapScore[EAST] != nNewEast)
		{
			g_mapScore[EAST] = nNewEast;
			bChange = true;
		}

		if (g_mapScore[SOUTH] != nNewSouth)
		{
			g_mapScore[SOUTH] = nNewSouth;
			bChange = true;
		}

		if (g_mapScore[WEST] != nNewWest)
		{
			g_mapScore[WEST] = nNewWest;
			bChange = true;
		}

		if (g_mapScore[NORTH] != nNewNorth)
		{
			g_mapScore[NORTH] = nNewNorth;
			bChange = true;
		}

		if (bChange)
			SendScore();

		std::this_thread::sleep_for(std::chrono::seconds(10));
	}


	g_bEndProgram = true;

	threadServer.join();

	close(fdServer);

	printf("finish\r\n");
	*/
	return 0;
}
