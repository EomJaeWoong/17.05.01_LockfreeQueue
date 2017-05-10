#include <Windows.h>
#include <stdio.h>
#include <process.h>
#include <time.h>
#include <stdlib.h>

#include "lib\Library.h"
#include "MemoryPool.h"
#include "LockfreeQueue.h"
#include "LockfreeQueue_Test.h"

CLockfreeQueue<st_TEST_DATA *> g_Queue;

LONG64 lPutTPS = 0;
LONG64 lGetTPS = 0;

LONG64 lPutCounter = 0;
LONG64 lGetCounter = 0;

unsigned __stdcall QueueThread(void *pParam);

void main()
{
	HANDLE hThread[dfTHREAD_MAX];
	DWORD dwThreadID;


	for (int iCnt = 0; iCnt < dfTHREAD_MAX; iCnt++)
	{
		hThread[iCnt] = (HANDLE)_beginthreadex(
			NULL,
			0,
			QueueThread,
			(LPVOID)1000,
			0,
			(unsigned int *)&dwThreadID
			);
	}

	while (1)
	{
		lPutTPS = lPutCounter;
		lGetTPS = lGetCounter;

		lPutCounter = 0;
		lGetCounter = 0;

		wprintf(L"------------------------------------------------\n");
		wprintf(L"Put TPS		: %d\n", lPutTPS);
		wprintf(L"Get TPS		: %d\n", lGetTPS);
		wprintf(L"Alloc TPS	: %d\n", g_Queue.GetMemAllocCount());
		wprintf(L"------------------------------------------------\n\n");
		if (g_Queue.GetMemAllocCount() > (dfTHREAD_MAX * dfTHREAD_ALLOC) + 1)
			LOG(L"MemoryPool", LOG::LEVEL_DEBUG, L"Over ] MemoryPool's AllocCount is overflow");

		Sleep(999);
	}
}

/*------------------------------------------------------------------*/
// 0. �� �����忡�� st_QUEUE_DATA �����͸� ���� ��ġ (10000��) ����		
// 0. ������ ����(Ȯ��)
// 1. iData = 0x0000000055555555 ����
// 1. lCount = 0 ����
// 2. ���ÿ� ����

// 3. �ణ���  Sleep (0 ~ 3)
// 4. ���� ���� ������ �� ��ŭ ���� 
// 4. - �̶� �����°� ���� ���� �������� ����, �ٸ� �����尡 ���� �������� ���� ����
// 5. ���� ��ü �����Ͱ� �ʱⰪ�� �´��� Ȯ��. (�����͸� ���� ����ϴ��� Ȯ��)
// 6. ���� ��ü �����Ϳ� ���� lCount Interlock + 1
// 6. ���� ��ü �����Ϳ� ���� iData Interlock + 1
// 7. �ణ���
// 8. + 1 �� �����Ͱ� ��ȿ���� Ȯ�� (���� �����͸� ���� ����ϴ��� Ȯ��)
// 9. ������ �ʱ�ȭ (0x0000000055555555, 0)
// 10. ���� �� ��ŭ ���ÿ� �ٽ� ����
//  3�� ���� �ݺ�.
/*------------------------------------------------------------------*/
unsigned __stdcall QueueThread(void *pParam)
{
	int iCnt;
	st_TEST_DATA *pData = NULL;
	st_TEST_DATA *pDataArray[dfTHREAD_ALLOC];

	// 0. ������ ����(Ȯ��)
	// 1. iData = 0x0000000055555555 ����
	// 1. lCount = 0 ����
	for (iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
	{
		pDataArray[iCnt] = new st_TEST_DATA;
		pDataArray[iCnt]->lData = 0x0000000055555555;
		pDataArray[iCnt]->lCount = 0;
	}

	// 2. ť�� ����
	for (iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
	{
		g_Queue.Put(pDataArray[iCnt]);
		InterlockedIncrement64(&lPutCounter);
	}

	while (1){
		// 3. �ణ���  Sleep (0 ~ 3)
		Sleep(3);

		// 4. ���� ���� ������ �� ��ŭ ���� 
		// 4. - �̶� �����°� ���� ���� �������� ����, �ٸ� �����尡 ���� �������� ���� ����
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			g_Queue.Get(&pData);
			InterlockedIncrement64(&lGetCounter);

			pDataArray[iCnt] = pData;
		}

		// 5. ���� ��ü �����Ͱ� �ʱⰪ�� �´��� Ȯ��. (�����͸� ���� ����ϴ��� Ȯ��)
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			if ((pDataArray[iCnt]->lData != 0x0000000055555555) || (pDataArray[iCnt]->lCount != 0))
				LOG(L"LockfreeStack", LOG::LEVEL_DEBUG, L"Get  ] pDataArray[%04d] is using in Queue\n", iCnt);
		}

		// 6. ���� ��ü �����Ϳ� ���� lCount Interlock + 1
		// 6. ���� ��ü �����Ϳ� ���� iData Interlock + 1
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			InterlockedIncrement64(&pDataArray[iCnt]->lCount);
			InterlockedIncrement64(&pDataArray[iCnt]->lData);
		}

		// 7. �ణ���
		Sleep(3);

		// 8. + 1 �� �����Ͱ� ��ȿ���� Ȯ�� (���� �����͸� ���� ����ϴ��� Ȯ��)
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			if ((pDataArray[iCnt]->lCount != 1) || (pDataArray[iCnt]->lData != 0x0000000055555556))
				LOG(L"LockfreeStack", LOG::LEVEL_DEBUG, L"Plus ] pDataArray[%04d] is using in Queue\n", iCnt);
		}

		// 9. ������ �ʱ�ȭ (0x0000000055555555, 0)
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			pDataArray[iCnt]->lData = 0x0000000055555555;
			pDataArray[iCnt]->lCount = 0;
		}

		// 10. ���� �� ��ŭ ť�� �ٽ� ����
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			g_Queue.Put(pDataArray[iCnt]);
			InterlockedIncrement64(&lPutCounter);
		}
	}
}