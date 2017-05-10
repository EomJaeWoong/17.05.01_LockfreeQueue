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
// 0. 각 스레드에서 st_QUEUE_DATA 데이터를 일정 수치 (10000개) 생성		
// 0. 데이터 생성(확보)
// 1. iData = 0x0000000055555555 셋팅
// 1. lCount = 0 셋팅
// 2. 스택에 넣음

// 3. 약간대기  Sleep (0 ~ 3)
// 4. 내가 넣은 데이터 수 만큼 뽑음 
// 4. - 이때 뽑히는건 내가 넣은 데이터일 수도, 다른 스레드가 넣은 데이터일 수도 있음
// 5. 뽑은 전체 데이터가 초기값과 맞는지 확인. (데이터를 누가 사용하는지 확인)
// 6. 뽑은 전체 데이터에 대해 lCount Interlock + 1
// 6. 뽑은 전체 데이터에 대해 iData Interlock + 1
// 7. 약간대기
// 8. + 1 한 데이터가 유효한지 확인 (뽑은 데이터를 누가 사용하는지 확인)
// 9. 데이터 초기화 (0x0000000055555555, 0)
// 10. 뽑은 수 만큼 스택에 다시 넣음
//  3번 으로 반복.
/*------------------------------------------------------------------*/
unsigned __stdcall QueueThread(void *pParam)
{
	int iCnt;
	st_TEST_DATA *pData = NULL;
	st_TEST_DATA *pDataArray[dfTHREAD_ALLOC];

	// 0. 데이터 생성(확보)
	// 1. iData = 0x0000000055555555 셋팅
	// 1. lCount = 0 셋팅
	for (iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
	{
		pDataArray[iCnt] = new st_TEST_DATA;
		pDataArray[iCnt]->lData = 0x0000000055555555;
		pDataArray[iCnt]->lCount = 0;
	}

	// 2. 큐에 넣음
	for (iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
	{
		g_Queue.Put(pDataArray[iCnt]);
		InterlockedIncrement64(&lPutCounter);
	}

	while (1){
		// 3. 약간대기  Sleep (0 ~ 3)
		Sleep(3);

		// 4. 내가 넣은 데이터 수 만큼 뽑음 
		// 4. - 이때 뽑히는건 내가 넣은 데이터일 수도, 다른 스레드가 넣은 데이터일 수도 있음
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			g_Queue.Get(&pData);
			InterlockedIncrement64(&lGetCounter);

			pDataArray[iCnt] = pData;
		}

		// 5. 뽑은 전체 데이터가 초기값과 맞는지 확인. (데이터를 누가 사용하는지 확인)
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			if ((pDataArray[iCnt]->lData != 0x0000000055555555) || (pDataArray[iCnt]->lCount != 0))
				LOG(L"LockfreeStack", LOG::LEVEL_DEBUG, L"Get  ] pDataArray[%04d] is using in Queue\n", iCnt);
		}

		// 6. 뽑은 전체 데이터에 대해 lCount Interlock + 1
		// 6. 뽑은 전체 데이터에 대해 iData Interlock + 1
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			InterlockedIncrement64(&pDataArray[iCnt]->lCount);
			InterlockedIncrement64(&pDataArray[iCnt]->lData);
		}

		// 7. 약간대기
		Sleep(3);

		// 8. + 1 한 데이터가 유효한지 확인 (뽑은 데이터를 누가 사용하는지 확인)
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			if ((pDataArray[iCnt]->lCount != 1) || (pDataArray[iCnt]->lData != 0x0000000055555556))
				LOG(L"LockfreeStack", LOG::LEVEL_DEBUG, L"Plus ] pDataArray[%04d] is using in Queue\n", iCnt);
		}

		// 9. 데이터 초기화 (0x0000000055555555, 0)
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			pDataArray[iCnt]->lData = 0x0000000055555555;
			pDataArray[iCnt]->lCount = 0;
		}

		// 10. 뽑은 수 만큼 큐에 다시 넣음
		for (int iCnt = 0; iCnt < dfTHREAD_ALLOC; iCnt++)
		{
			g_Queue.Put(pDataArray[iCnt]);
			InterlockedIncrement64(&lPutCounter);
		}
	}
}