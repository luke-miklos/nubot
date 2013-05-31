// TestMain.cpp : Defines the entry point for the console application.
//

#pragma once
//#ifndef _WIN32_WINNT            // Specifies that the minimum required platform is Windows Vista.
//#define _WIN32_WINNT 0x0600     // Change this to the appropriate value to target other versions of Windows.
//#endif
#include <stdio.h>
#include <tchar.h>
#include "MacroSearch.h"

#include <sys/timeb.h>

int getMilliCount()
{
   timeb tb;
   ftime(&tb);
   int nCount = tb.millitm + (tb.time & 0xfffff) * 1000;
   return nCount;
}

int getMilliSpan(int nTimeStart)
{
   int nSpan = getMilliCount() - nTimeStart;
   if(nSpan < 0)
      nSpan += 0x100000 * 1000;
   return nSpan;
}


int _tmain(int argc, _TCHAR* argv[])
{
   printf("testing 123");

   MacroSearch* macro = new MacroSearch();

   int start = getMilliCount();
      std::vector<QueuedMove*> moves = macro->FindMoves(4320);  //3.0 * 60 * 24
   int milliSecondsElapsed = getMilliSpan(start);
   printf("\n\nDepth 3.0, Elapsed time = %u milliseconds\n", milliSecondsElapsed);

   start = getMilliCount();
      moves = macro->FindMoves(5040);  //3.5 * 60 * 24
   milliSecondsElapsed = getMilliSpan(start);
   printf("\n\nDepth 3.5, Elapsed time = %u milliseconds\n", milliSecondsElapsed);

   start = getMilliCount();
      moves = macro->FindMoves(5760);  //4.0 * 60 * 24
   milliSecondsElapsed = getMilliSpan(start);
   printf("\n\nDepth 4.0, Elapsed time = %u milliseconds\n", milliSecondsElapsed);

   start = getMilliCount();
      moves = macro->FindMoves(6120);  //4.25 * 60 * 24
   milliSecondsElapsed = getMilliSpan(start);
   printf("\n\nDepth 4.25, Elapsed time = %u milliseconds\n", milliSecondsElapsed);

   int temp = 0;
   scanf_s("%d", &temp);
   return 0;
}

