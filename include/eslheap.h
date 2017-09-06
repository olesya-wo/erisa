#if	!defined(__ESLHEAP_H__)
#define	__ESLHEAP_H__


//////////////////////////////////////////////////////////////////////////////
// Описание данных
//////////////////////////////////////////////////////////////////////////////

typedef	struct tagHSTACKHEAP *	HSTACKHEAP ;
typedef	struct tagHESLHEAP *	HESLHEAP ;


//////////////////////////////////////////////////////////////////////////////
// Описание функций
//////////////////////////////////////////////////////////////////////////////

//
// Функции управления памятью
//

void eslFillMemory( void * ptrMem, BYTE bytData, DWORD dwLength ) ;
void eslMoveMemory( void * ptrDst, const void * ptrSrc, DWORD dwLength ) ;

//
// Общее
//
HESLHEAP eslGetGlobalHeap( void ) ;
HESLHEAP eslHeapCreate( DWORD dwInitSize = 0, DWORD dwGrowSize = 0,DWORD dwFlags = 0, HESLHEAP hParentHeap = NULL ) ;
void eslHeapDestroy( HESLHEAP hHeap ) ;
void * eslHeapAllocate( HESLHEAP hHeap, DWORD dwSize, DWORD dwFlags ) ;
void eslHeapFree( HESLHEAP hHeap, void * ptrObj, DWORD dwFlags = 0 ) ;
void * eslHeapReallocate( HESLHEAP hHeap, void * ptrObj, DWORD dwSize, DWORD dwFlags ) ;
DWORD eslHeapGetLength( HESLHEAP hHeap, void * ptrObj ) ;
ESLError eslVerifyHeapChain( HESLHEAP hHeap ) ;
void eslHeapLock( HESLHEAP hHeap ) ;
void eslHeapUnlock( HESLHEAP hHeap ) ;

#define	ESL_HEAP_ZERO_INIT		0x00000001
#define	ESL_HEAP_NO_SERIALIZE	0x00000002
#define	ESL_INVALID_HEAP		(0xFFFFFFFFUL)


#endif

