
/*****************************************************************************
           ESL (Entis Standard Library) heap core implementation
 ----------------------------------------------------------------------------
        Copyright (c) 2002-2004 Leshade Entis. All rights reserved.
 *****************************************************************************/


#define	STRICT	1
#include <stdio.h>
#include "xerisa.h"
#include "esl.h"
#include <string.h>

// Управление виртуальной памятью - единственное платформенно-зависимое место
#ifdef _WIN32
#include <windows.h>
#define page_alloc(size) VirtualAlloc( NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE )
#define page_free(addr) VirtualFree( addr, 0, MEM_RELEASE )
#else
#include <sys/mman.h>
#define page_alloc(size) mmap( NULL, size, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_ANONYMOUS, -1, 0 )
#define page_free(addr) munmap( addr, 0 ) // Size!
#endif


//////////////////////////////////////////////////////////////////////////////
// デバッグサポート関数
//////////////////////////////////////////////////////////////////////////////

const char * GetESLErrorMsg( ESLError err )
{
	switch ( err )
	{
	case	eslErrSuccess:
		return	"関数は成功しました。" ;
	case	eslErrNotSupported:
		return	"未対応の機能を利用しようとしました。" ;
	case	eslErrGeneral:
		return	"一般的なエラーが発生しました。" ;
	case	eslErrAbort:
		return	"処理が中断されました。" ;
	case	eslErrInvalidParam:
		return	"不正なパラメータが指定されました。" ;
	case	eslErrTimeout:
		return	"関数はタイムアウトしました。" ;
	case	eslErrPending:
		return	"処理がペンディング状態です。" ;
	case	eslErrContinue:
		return	"処理は継続中です。" ;
	}
	if ( err & 0xFFFF0000 )
	{
		return	(const char *) err ;
	}
	else
	{
		return	"未知のエラーコードです。" ;
	}
}


//////////////////////////////////////////////////////////////////////////////
// 基底クラス
//////////////////////////////////////////////////////////////////////////////

const char *const	ESLObject::m_pszClassName = "ESLObject" ;

void * ESLObject::operator new ( size_t stObj )
{
	return	::eslHeapAllocate( NULL, stObj, 0 ) ;
}

void * ESLObject::operator new ( size_t stObj, void * ptrObj )
{
	return	ptrObj ;
}

void * ESLObject::operator new
	( size_t stObj, const char * pszFileName, int nLine )
{
	return	::eslHeapAllocate( NULL, stObj, 0 ) ;
}

void ESLObject::operator delete ( void * ptrObj )
{
	::eslHeapFree( NULL, ptrObj, 0 ) ;
}


//////////////////////////////////////////////////////////////////////////////
// スタック式ヒープ関数
//////////////////////////////////////////////////////////////////////////////

struct	tagHSTACKHEAP
{
	HSTACKHEAP		pNextHeap ;			// 次のヒープ
	HSTACKHEAP		pCurrentHeap ;		// カレントヒープ
	DWORD			dwGrowSize ;		// 拡張サイズ
	BYTE *			pbytBeginBlock ;	// メモリブロックの先頭
	DWORD			dwLimitBytes ;		// ヒープの限界サイズ
	DWORD			dwUsedBytes ;		// 使用されているバイト数
	DWORD			for_align[2] ;
} ;

typedef	tagHSTACKHEAP	STACK_HEAP_HEADER ;

//////////////////////////////////////////////////////////////////////////////
// 汎用ヒープヘッダ構造体
//////////////////////////////////////////////////////////////////////////////

struct	VAR_HEAP_BLOCK
{
	DWORD					dwSize ;		// メモリブロックの有効なバイト数
	union
	{
		VAR_HEAP_BLOCK *	pPrevious ;		// 直前のメモリブロックのアドレス
		HESLHEAP			hHeap ;			// 属するヒープ
	} ;
} ;

#define	VAR_HEAP_SIZE_MASK	(0x0FFFFFFFUL)
#define	VAR_HEAP_FLAG_MASK	(0xF0000000UL)
#define	VAR_HEAP_FREE_FLAG	(0x80000000UL)
#define	VAR_HEAP_LAST_FLAG	(0x40000000UL)
#define	VAR_HEAP_FIRST_FLAG	(0x20000000UL)
#define	VAR_HEAP_SIGNATURE	(0x10000000UL)
#define	HEAP_LEN_SCALE		2

struct	VAR_FREE_BLOCK_INFO
{
	VAR_HEAP_BLOCK *	pPrevious ;		// 前の未確保ブロック
	VAR_HEAP_BLOCK *	pNext ;			// 次の未確保ブロック
} ;

#define	VAR_FREE_BLOCK_BASE	((sizeof(VAR_HEAP_BLOCK) + sizeof(VAR_FREE_BLOCK_INFO)) * 2)

struct	tagHESLHEAP
{
	HESLHEAP			pNextHeap ;		// 次のヒープ
	HESLHEAP			pPrevHeap ;		// 前のヒープ

	VAR_HEAP_BLOCK *	pFirstSpace ;	// 初めの未確保ブロック
	DWORD				dwFreeSpace ;	// 未確保領域の合計
	DWORD				dwMaxFreeArea ;	// 連続した最大のみ確保領域のサイズ
										// ※この値は正確ではない
										// 　確保できるヒープを探すために
										// 　用いられるが、正確な数以上の値で
										// 　出来るだけ小さな値が望ましい

	DWORD				dwGrowSize ;	// ヒープの拡張サイズ
	HESLHEAP			pLastHeap ;		// 最後のヒープ

	DWORD				dwDefFlags ;	// デフォルトフラグ
	DWORD				dwHeapSize ;	// ヒープのサイズ

	DWORD				dwRefCount ;	// 参照カウント
	HESLHEAP			hParentHeap ;	// 親ヒープ
	PVOID				pOrgMemAddr ;	// 子ヒープの元のメモリブロックアドレス
} ;

typedef	tagHESLHEAP	VAR_HEAP_HEADER ;

inline void * GetBlockAddress( const VAR_HEAP_BLOCK * pBlock )
{
	return	((BYTE*) pBlock) + sizeof(VAR_HEAP_BLOCK) ;
}

inline DWORD GetBlockLength( const VAR_HEAP_BLOCK * pBlock )
{
	ESLAssert( !(pBlock->dwSize & (0x07 >> HEAP_LEN_SCALE)) ) ;
	return	((pBlock->dwSize & VAR_HEAP_SIZE_MASK) << HEAP_LEN_SCALE) ;
}

inline VAR_HEAP_BLOCK * GetNextBlock( const VAR_HEAP_BLOCK * pBlock )
{
	return	(VAR_HEAP_BLOCK*) (((BYTE*)pBlock)
				+ GetBlockLength(pBlock) + sizeof(VAR_HEAP_BLOCK)) ;
}

inline VAR_FREE_BLOCK_INFO * GetFreeBlockInfo( const VAR_HEAP_BLOCK * pBlock )
{
	return	(VAR_FREE_BLOCK_INFO*) (((BYTE*)pBlock) + sizeof(VAR_HEAP_BLOCK)) ;
//	return	(VAR_FREE_BLOCK_INFO*)
//		(((BYTE*)pBlock) + GetBlockLength(pBlock)
//				+ (sizeof(VAR_HEAP_BLOCK) - sizeof(VAR_FREE_BLOCK_INFO))) ;
}

static HESLHEAP	g_hEslHeap = NULL ;			// グローバルヒープ


//////////////////////////////////////////////////////////////////////////////
// ヒープチェーンの検証
//////////////////////////////////////////////////////////////////////////////

ESLError eslVerifyHeapChain( HESLHEAP hHeap )
{
	if ( hHeap == NULL )
	{
		hHeap = g_hEslHeap ;
	}
	HESLHEAP	hPrev = hHeap ;
	HESLHEAP	hNext ;
    try
	{
		for ( ; ; )
		{
			hNext = hPrev->pNextHeap ;
			if ( hNext == NULL )
			{
//				ESLAssert( (hHeap->pLastHeap == hPrev) || (hPrev == hHeap) ) ;
				if ( (hHeap->pLastHeap == hPrev) || (hPrev == hHeap) )
				{
					break ;
				}
                //__asm	int	3
				return	eslErrGeneral ;
			}
//			ESLAssert( hNext->pPrevHeap == hPrev ) ;
			if ( hNext->pPrevHeap != hPrev )
			{
                //__asm	int	3
				return	eslErrGeneral ;
			}
			hPrev = hNext ;
		}
	}
    catch(...)
	{
		return	eslErrGeneral ;
	}
	return	eslErrSuccess ;
}


//////////////////////////////////////////////////////////////////////////////
// グローバルヒープ取得
//////////////////////////////////////////////////////////////////////////////

HESLHEAP eslGetGlobalHeap( void )
{
	if ( g_hEslHeap == NULL )
	{
		g_hEslHeap = ::eslHeapCreate( 0, 0, 0 ) ;
	}
	return	g_hEslHeap ;
}



//////////////////////////////////////////////////////////////////////////////
// 汎用ヒープ生成
//////////////////////////////////////////////////////////////////////////////

HESLHEAP eslHeapCreate
	( DWORD dwInitSize, DWORD dwGrowSize,
			DWORD dwFlags, HESLHEAP hParentHeap )
{
	//
	// メモリを確保する
	//
	if ( dwInitSize == 0 )
	{
		dwInitSize = 0x10000 ;
	}
	else
	{
		dwInitSize = (dwInitSize + 0x0FFF) & (~0x0FFF) ;
	}
	if ( dwGrowSize == 0 )
	{
		dwGrowSize = 0x10000 ;
	}
	//
	VAR_HEAP_HEADER *	pHeapHdr ;
	for ( ; ; )
	{
		if ( hParentHeap == NULL )
		{
            pHeapHdr = (VAR_HEAP_HEADER*) page_alloc(dwInitSize);
		}
		else
		{
			PVOID	pOrgMemAddr = ::eslHeapAllocate
				( hParentHeap, dwInitSize + 0x10, ESL_HEAP_ZERO_INIT ) ;
			pHeapHdr = (VAR_HEAP_HEADER*)
						((((DWORD)pOrgMemAddr) + 0x0F) & ~0x0F) ;
			pHeapHdr->pOrgMemAddr = pOrgMemAddr ;
		}
		if ( pHeapHdr != NULL )
		{
			break ;
		}
        return	NULL ;
	}
	//
	// ヒープを初期化
	//
	pHeapHdr->pNextHeap = NULL ;
	pHeapHdr->pPrevHeap = NULL ;
	pHeapHdr->dwGrowSize = dwGrowSize ;
	pHeapHdr->pLastHeap = NULL ;
	pHeapHdr->dwDefFlags = dwFlags ;
	pHeapHdr->dwHeapSize = dwInitSize ;
	pHeapHdr->dwRefCount = 1 ;
	pHeapHdr->hParentHeap = hParentHeap ;
	//
	if ( hParentHeap != NULL )
	{
		hParentHeap->dwRefCount ++ ;
	}
	//
	//
	VAR_HEAP_BLOCK *	pBlock =
		(VAR_HEAP_BLOCK*) (((DWORD)pHeapHdr) + sizeof(VAR_HEAP_HEADER)) ;
	pHeapHdr->pFirstSpace = pBlock ;
	pHeapHdr->dwFreeSpace =
		dwInitSize - sizeof(VAR_HEAP_HEADER) - sizeof(VAR_HEAP_BLOCK) ;
	pHeapHdr->dwMaxFreeArea = pHeapHdr->dwFreeSpace ;
	//
	pBlock->dwSize =
		(pHeapHdr->dwFreeSpace >> HEAP_LEN_SCALE)
			| VAR_HEAP_SIGNATURE | VAR_HEAP_FREE_FLAG
			| VAR_HEAP_LAST_FLAG | VAR_HEAP_FIRST_FLAG ;
	pBlock->hHeap = pHeapHdr ;
	//
	VAR_FREE_BLOCK_INFO *	pvfbi = GetFreeBlockInfo( pBlock ) ;
	pvfbi->pNext = NULL ;
	pvfbi->pPrevious = NULL ;
	//
	return	pHeapHdr ;
}


//////////////////////////////////////////////////////////////////////////////
// 汎用ヒープ削除
//////////////////////////////////////////////////////////////////////////////

void eslHeapDestroy( HESLHEAP hHeap )
{
	if ( hHeap != NULL )
	{
		if ( (-- hHeap->dwRefCount) == 0 )
		{
			//
			HESLHEAP	hLeadHeap = hHeap ;
			while ( hHeap )
			{
				HESLHEAP	hNext = hHeap->pNextHeap ;
				HESLHEAP	hParent = hHeap->hParentHeap ;
				if ( hParent != NULL )
				{
					::eslHeapFree( hParent, hHeap->pOrgMemAddr, 0 ) ;
					::eslHeapDestroy( hParent ) ;
				}
				else
				{
                    page_free(hHeap);
				}
				hHeap = hNext ;
			}
			//
			if ( g_hEslHeap == hLeadHeap )
			{
				g_hEslHeap = NULL ;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////
// 汎用ヒープにメモリを確保
//////////////////////////////////////////////////////////////////////////////

void * eslHeapAllocate
	( HESLHEAP hHeap, DWORD dwSize, DWORD dwFlags )
{
	if ( hHeap == NULL )
	{
		hHeap = eslGetGlobalHeap( ) ;
	}
	//
	// サイズを検証する
	//
	bool				fLargeMemory = false ;
	HESLHEAP			hTarget = hHeap ;
	VAR_HEAP_BLOCK *	pBlock = NULL ;
	dwSize = ((dwSize + 0x07 + (dwSize == 0)) & (~0x07)) ;
    dwSize &= ~(VAR_HEAP_FLAG_MASK << HEAP_LEN_SCALE) ;
	//
	dwFlags |= hHeap->dwDefFlags ;
	//
	ESLAssert( dwSize != 0 ) ;
	if ( (hHeap->hParentHeap == NULL) && (dwSize >= (hHeap->dwGrowSize / 2)) )
	{
		fLargeMemory = true ;
		DWORD	dwHeapSize =
			dwSize + sizeof(VAR_HEAP_HEADER)
				+ sizeof(VAR_HEAP_BLOCK)
				+ sizeof(VAR_FREE_BLOCK_BASE) * 2 + 0x20 ;
		dwHeapSize = (dwHeapSize + 0x0FFF) & (~0x0FFF) ;
		hTarget = eslHeapCreate( dwHeapSize, 0, ESL_HEAP_NO_SERIALIZE ) ;
		if ( hTarget == NULL )
		{
			return	NULL ;
		}
		if ( hHeap->pLastHeap != NULL )
		{
			ESLAssert( hHeap->pLastHeap->pNextHeap == NULL ) ;
			hTarget->pPrevHeap = hHeap->pLastHeap ;
			hHeap->pLastHeap->pNextHeap = hTarget ;
		}
		else
		{
			hHeap->pNextHeap = hTarget ;
			hTarget->pPrevHeap = hHeap ;
		}
		hHeap->pLastHeap = hTarget ;
	}
	//
	// アロケーション可能なヒープを探す
	//
	for ( ; ; )
	{
		ESLAssert( pBlock == NULL ) ;
		//
		// アロケーション可能な未確保領域を探す
		//
		VAR_HEAP_BLOCK *	pNext = hTarget->pFirstSpace ;
		DWORD	dwMaxFreeArea = hTarget->dwMaxFreeArea ;
		DWORD	dwFreeSize = hTarget->dwFreeSpace - VAR_FREE_BLOCK_BASE ;
		DWORD	dwNeedSize = dwSize + VAR_FREE_BLOCK_BASE ;
		//
		if ( dwMaxFreeArea >= dwSize )
		{
			DWORD	dwCurMaxFree = 0, dwBlockSize, dwCurrentNeed ;
			SDWORD	dwMaxMask ;
			//
			while ( dwFreeSize >= dwSize )
			{
				ESLAssert( pNext && (pNext->dwSize & VAR_HEAP_FREE_FLAG) ) ;
				//
				dwBlockSize = GetBlockLength( pNext ) ;
				dwMaxMask = ((SDWORD)(dwCurMaxFree - dwBlockSize)) >> 31 ;
				dwCurrentNeed = dwNeedSize
					- ((- (int) !(pNext->dwSize & VAR_HEAP_FIRST_FLAG))
							& (VAR_FREE_BLOCK_BASE - sizeof(VAR_HEAP_BLOCK))) ;
				dwCurMaxFree =
					(dwBlockSize & dwMaxMask) | (dwCurMaxFree & ~dwMaxMask) ;
				//
				if ( dwBlockSize >= dwCurrentNeed )
				{
					pBlock = pNext ;
					break ;
				}
				if ( pNext->dwSize & VAR_HEAP_LAST_FLAG )
				{
					break ;
				}
				pNext = GetFreeBlockInfo(pNext)->pNext ;
				if ( pNext == NULL )
				{
					break ;
				}
				dwFreeSize -= dwBlockSize ;
			}
			// dwCurMaxFree = max( dwFreeSize, dwCurMaxFree ) ;
			dwMaxMask = ((SDWORD)(dwFreeSize - dwCurMaxFree)) >> 31 ;
			dwCurMaxFree =
				(dwCurMaxFree & dwMaxMask) | (dwFreeSize & ~dwMaxMask) ;
			// hTarget->dwMaxFreeArea = min( dwMaxFreeArea, dwCurMaxFree ) ;
			dwMaxMask = ((SDWORD)(dwMaxFreeArea - dwCurMaxFree)) >> 31 ;
			hTarget->dwMaxFreeArea =
				(dwMaxFreeArea & dwMaxMask) | (dwCurMaxFree & ~dwMaxMask) ;
		}
		if ( pBlock != NULL )
		{
			break ;
		}
		//
		// 次のヒープへ
		//
		hTarget = hTarget->pNextHeap ;
		//
		if ( (hTarget == NULL) || (hTarget->pFirstSpace == NULL) )
		{
			//
			// ヒープを拡張する
			//
			ESLAssert( !fLargeMemory ) ;
			DWORD	dwHeapSize =
				dwSize + sizeof(VAR_HEAP_HEADER) * 2
						+ sizeof(VAR_FREE_BLOCK_BASE) * 2 ;
			if ( (hHeap->hParentHeap == NULL)
					&& (dwHeapSize < hHeap->dwGrowSize) )
			{
				dwHeapSize = hHeap->dwGrowSize ;
			}
			dwHeapSize = (dwHeapSize + 0x0FFF) & (~0x0FFF) ;
			//
			hTarget = eslHeapCreate
				( dwHeapSize, 0, ESL_HEAP_NO_SERIALIZE, hHeap->hParentHeap ) ;
			if ( hTarget == NULL )
			{
				return	NULL ;
			}
			if ( hHeap->pNextHeap != NULL )
			{
				hTarget->pPrevHeap = hHeap ;
				hTarget->pNextHeap = hHeap->pNextHeap ;
				hHeap->pNextHeap->pPrevHeap = hTarget ;
				hHeap->pNextHeap = hTarget ;
			}
			else
			{
				hHeap->pNextHeap = hHeap->pLastHeap = hTarget ;
				hTarget->pPrevHeap = hHeap ;
			}
		}
	}
	//
	// 未確保領域にメモリを割り当てる
	//
	BYTE *	ptrObj = NULL ;
	//
	if ( fLargeMemory )
	{
		DWORD	dwAllocSize =
			(GetBlockLength(pBlock) - VAR_FREE_BLOCK_BASE) & (~0x0F) ;
		ESLAssert( dwAllocSize >= dwSize ) ;
		ESLAssert( GetBlockLength(pBlock) >= dwAllocSize + VAR_FREE_BLOCK_BASE ) ;
		dwSize = dwAllocSize ;
		hTarget->pFirstSpace = NULL ;
	}
	if ( (pBlock->dwSize & VAR_HEAP_FIRST_FLAG) ||
			(GetBlockLength(pBlock) >= dwSize + VAR_FREE_BLOCK_BASE) )
	{
		//
		// ブロックを分割する
		//
		DWORD	dwBlockSize = GetBlockLength(pBlock) ;
		DWORD	dwNewSize =
			dwBlockSize - (dwSize + sizeof(VAR_HEAP_BLOCK)) ;
		//
		ESLAssert( (signed long int) dwNewSize > 0L ) ;
		//
//		VAR_FREE_BLOCK_INFO	vfbi = *(GetFreeBlockInfo(pBlock)) ;
		pBlock->dwSize = (dwNewSize >> HEAP_LEN_SCALE)
						| (pBlock->dwSize & VAR_HEAP_FLAG_MASK) ;
//		*(GetFreeBlockInfo(pBlock)) = vfbi ;
		//
		VAR_HEAP_BLOCK *	pNext = GetNextBlock( pBlock ) ;
		pNext->dwSize = (dwSize >> HEAP_LEN_SCALE) | VAR_HEAP_SIGNATURE ;
		pNext->pPrevious = pBlock ;
		//
		if ( pBlock->dwSize & VAR_HEAP_LAST_FLAG )
		{
			pBlock->dwSize &= ~VAR_HEAP_LAST_FLAG ;
			pNext->dwSize |= VAR_HEAP_LAST_FLAG ;
		}
		else
		{
			GetNextBlock(pNext)->pPrevious = pNext ;
		}
		//
		ptrObj = (BYTE*) GetBlockAddress( pNext ) ;
		hTarget->dwFreeSpace -= (dwSize + sizeof(VAR_HEAP_BLOCK)) ;
	}
	else
	{
		//
		// ブロックをそのまま割り当てる
		//
		VAR_FREE_BLOCK_INFO *	pvfbi = GetFreeBlockInfo( pBlock ) ;
		if ( pvfbi->pNext != NULL )
		{
			VAR_FREE_BLOCK_INFO *
				pvfbiNext = GetFreeBlockInfo( pvfbi->pNext ) ;
			pvfbiNext->pPrevious = pvfbi->pPrevious ;
		}
		GetFreeBlockInfo(pvfbi->pPrevious)->pNext = pvfbi->pNext ;
		//
		pBlock->dwSize &= ~VAR_HEAP_FREE_FLAG ;
		//
		ptrObj = (BYTE*) GetBlockAddress( pBlock ) ;
		hTarget->dwFreeSpace -= GetBlockLength( pBlock ) ;
	}
	//
	if ( dwFlags & ESL_HEAP_ZERO_INIT )
	{
		::eslFillMemory( ptrObj, 0, dwSize ) ;
	}
	//
	//
	return	ptrObj ;
}


//////////////////////////////////////////////////////////////////////////////
// 汎用ヒープからメモリブロックを解放
//////////////////////////////////////////////////////////////////////////////

void eslHeapFree( HESLHEAP hHeap, void * ptrObj, DWORD dwFlags )
{
	//
	// ポインタの有効性の検証
	//
	ESLAssert( !(((DWORD)ptrObj) & (0x07 >> HEAP_LEN_SCALE)) ) ;
	if ( ptrObj == NULL )
	{
		return ;
	}
	VAR_HEAP_BLOCK *	pBlock =
		(VAR_HEAP_BLOCK*) (((DWORD) ptrObj) - sizeof(VAR_HEAP_BLOCK)) ;
	if ( (pBlock->dwSize & (VAR_HEAP_FREE_FLAG | VAR_HEAP_FIRST_FLAG))
		|| !(pBlock->dwSize & VAR_HEAP_SIGNATURE) )
	{
		ESLTrace( "無効なポインタを解放しようとしました。\n" ) ;
		return ;
	}
	//
	// 解放処理
	//
	if ( hHeap == NULL )
	{
		hHeap = g_hEslHeap ;
	}
	dwFlags |= hHeap->dwDefFlags ;
	//
	// ヒープヘッダを取得する
	//
	HESLHEAP	hTarget = NULL ;
	if ( pBlock->dwSize & VAR_HEAP_FIRST_FLAG )
	{
		hTarget = pBlock->hHeap ;
	}
	else if ( pBlock->pPrevious->dwSize & VAR_HEAP_FIRST_FLAG )
	{
		hTarget = pBlock->pPrevious->hHeap ;
	}
	else
	{
		HESLHEAP	hNextHeap = hHeap ;
		DWORD	dwBlockAddr = (DWORD) pBlock ;
		while ( hNextHeap != NULL )
		{
			DWORD	dwHeapAddr = (DWORD) hNextHeap ;
			if ( (dwHeapAddr <= dwBlockAddr)
				&& (dwBlockAddr < dwHeapAddr + hNextHeap->dwHeapSize) )
			{
				break ;
			}
			HESLHEAP	hPrevHeap = hNextHeap ;
			hNextHeap = hPrevHeap->pNextHeap ;
			ESLAssert( (hNextHeap != NULL) && (hNextHeap->pPrevHeap == hPrevHeap) ) ;
		}
		ESLAssert( hNextHeap != NULL ) ;
		if ( hNextHeap == NULL )
		{
			ESLTrace( "無効なメモリブロックをヒープから解放しようとしました。\n" ) ;
			return ;
		}
		hTarget = hNextHeap ;
	}
	//
	// 現在のブロックを解放して直前の未確保ブロックと連結
	//
	VAR_FREE_BLOCK_INFO	vfbi ;
	if ( pBlock->pPrevious->dwSize & VAR_HEAP_FREE_FLAG )
	{
		//
		// 直前のブロックは未確保領域 -> 連結
		//
		VAR_HEAP_BLOCK *	pPrev ;
		pPrev = pBlock->pPrevious ;
		ESLAssert( GetNextBlock(pPrev) == pBlock ) ;
//		vfbi = *(GetFreeBlockInfo(pPrev)) ;
		//
		DWORD	dwFlags = pPrev->dwSize & VAR_HEAP_FLAG_MASK ;
		DWORD	dwFreeSize =
			GetBlockLength( pBlock ) + sizeof(VAR_HEAP_BLOCK) ;
		hTarget->dwFreeSpace += dwFreeSize ;
		pPrev->dwSize =
			((GetBlockLength(pPrev) + dwFreeSize)
									>> HEAP_LEN_SCALE) | dwFlags ;
		if ( pBlock->dwSize & VAR_HEAP_LAST_FLAG )
		{
			pPrev->dwSize |= VAR_HEAP_LAST_FLAG ;
		}
		else
		{
			ESLAssert( !(pPrev->dwSize & VAR_HEAP_LAST_FLAG) ) ;
			GetNextBlock(pPrev)->pPrevious = pPrev ;
		}
		pBlock = pPrev ;
	}
	else
	{
		//
		// 現在のブロックを未確保に設定
		//
		ESLAssert( hTarget->pFirstSpace != NULL ) ;
		VAR_FREE_BLOCK_INFO *
			pFirst = GetFreeBlockInfo( hTarget->pFirstSpace ) ;
		vfbi.pPrevious = hTarget->pFirstSpace ;
		vfbi.pNext = pFirst->pNext ;
		pFirst->pNext = pBlock ;
		//
		if ( vfbi.pNext != NULL )
		{
			VAR_FREE_BLOCK_INFO *
				pNext = GetFreeBlockInfo( vfbi.pNext ) ;
			pNext->pPrevious = pBlock ;
		}
		//
		pBlock->dwSize |= VAR_HEAP_FREE_FLAG ;
		hTarget->dwFreeSpace += GetBlockLength(pBlock) ;
		*(GetFreeBlockInfo(pBlock)) = vfbi ;
	}
	//
	// 未確保ブロック情報を設定
	//
//	*(GetFreeBlockInfo(pBlock)) = vfbi ;
	//
	// 現在のブロックと直後の未確保ブロックと連結
	//
	if ( !(pBlock->dwSize & VAR_HEAP_LAST_FLAG) )
	{
		VAR_HEAP_BLOCK *	pNext = GetNextBlock( pBlock ) ;
		if ( pNext->dwSize & VAR_HEAP_FREE_FLAG )
		{
			//
			// 直後のブロックは未確保領域 -> 連結
			//
			VAR_FREE_BLOCK_INFO *	pvfbi = GetFreeBlockInfo(pNext) ;
			if ( pvfbi->pNext != NULL )
			{
				VAR_FREE_BLOCK_INFO *
					pvfbiNext = GetFreeBlockInfo( pvfbi->pNext ) ;
				pvfbiNext->pPrevious = pvfbi->pPrevious ;
			}
			GetFreeBlockInfo(pvfbi->pPrevious)->pNext = pvfbi->pNext ;
			//
//			vfbi = *(GetFreeBlockInfo(pBlock)) ;
			//
			DWORD	dwFlags = pBlock->dwSize & VAR_HEAP_FLAG_MASK ;
			DWORD	dwFreeSize =
				GetBlockLength( pNext ) + sizeof(VAR_HEAP_BLOCK) ;
			hTarget->dwFreeSpace += sizeof(VAR_HEAP_BLOCK) ;
			pBlock->dwSize =
				((GetBlockLength( pBlock ) + dwFreeSize)
								>> HEAP_LEN_SCALE) | dwFlags ;
			if ( pNext->dwSize & VAR_HEAP_LAST_FLAG )
			{
				pBlock->dwSize |= VAR_HEAP_LAST_FLAG ;
			}
			else
			{
				ESLAssert( !(pBlock->dwSize & VAR_HEAP_LAST_FLAG) ) ;
				GetNextBlock(pBlock)->pPrevious = pBlock ;
			}
			//
//			*(GetFreeBlockInfo(pBlock)) = vfbi ;
		}
	}
	//
	// ヒープの連続最大確保可能サイズを更新する
	//
	DWORD	dwSize = GetBlockLength( pBlock ) ;
	SDWORD	dwMaxMask = ((SDWORD)(hTarget->dwMaxFreeArea - dwSize)) >> 31 ;
	hTarget->dwMaxFreeArea =
		(dwSize & dwMaxMask) | (hTarget->dwMaxFreeArea & ~dwMaxMask) ;
	//
	// ヒープそのものの解放
	//
	if ( hTarget != hHeap )
	{
		if ( (pBlock->dwSize & (VAR_HEAP_LAST_FLAG | VAR_HEAP_FIRST_FLAG))
								== (VAR_HEAP_LAST_FLAG | VAR_HEAP_FIRST_FLAG) )
		{
			bool	fFreeHeap = (hTarget->pFirstSpace == NULL) ;
			if ( !fFreeHeap )
			{
				//
				// ヒープを即時に解放するか判断
				//
				HESLHEAP	hNext = hHeap->pNextHeap ;
				fFreeHeap =
					(hNext != NULL) && (hNext != hTarget)
					&& (hNext->pFirstSpace != NULL)
					&& ((hNext->pFirstSpace->dwSize
							& (VAR_HEAP_LAST_FLAG | VAR_HEAP_FIRST_FLAG))
								== (VAR_HEAP_LAST_FLAG | VAR_HEAP_FIRST_FLAG)) ;
				//
				ESLAssert( hTarget->pPrevHeap->pNextHeap == hTarget ) ;
				hTarget->pPrevHeap->pNextHeap = hTarget->pNextHeap ;
				if ( hTarget->pNextHeap != NULL )
				{
					hTarget->pNextHeap->pPrevHeap = hTarget->pPrevHeap ;
				}
				if ( hHeap->pLastHeap == hTarget )
				{
					if ( hTarget->pPrevHeap != hHeap )
						hHeap->pLastHeap = hTarget->pPrevHeap ;
					else
						hHeap->pLastHeap = NULL ;
				}
				if ( hHeap->pNextHeap != NULL )
				{
					hTarget->pPrevHeap = hHeap ;
					hTarget->pNextHeap = hHeap->pNextHeap ;
					hHeap->pNextHeap->pPrevHeap = hTarget ;
					hHeap->pNextHeap = hTarget ;
				}
				else
				{
					hHeap->pNextHeap = hHeap->pLastHeap = hTarget ;
					hTarget->pPrevHeap = hHeap ;
				}
				//
				if ( fFreeHeap )
				{
					hTarget = hNext ;
				}
			} 
			if ( fFreeHeap )
			{
				//
				// ヒープチェーンを切断
				//
				ESLAssert( hTarget->pPrevHeap->pNextHeap == hTarget ) ;
				hTarget->pPrevHeap->pNextHeap = hTarget->pNextHeap ;
				if ( hTarget->pNextHeap != NULL )
				{
					hTarget->pNextHeap->pPrevHeap = hTarget->pPrevHeap ;
				}
				if ( hHeap->pLastHeap == hTarget )
				{
					if ( hTarget->pPrevHeap != hHeap )
						hHeap->pLastHeap = hTarget->pPrevHeap ;
					else
						hHeap->pLastHeap = NULL ;
				}
				//
				// ヒープを解放
				//
				if ( hTarget->hParentHeap == NULL )
				{
                    page_free(hTarget);
				}
				else
				{
//					HESLHEAP	hParent = hTarget->hParentHeap ;
					hTarget->pNextHeap = NULL ;
					hTarget->pPrevHeap = NULL ;
					hTarget->pLastHeap = NULL ;
					::eslHeapDestroy( hTarget ) ;
				}
			}
		}
		else if ( (hTarget->pPrevHeap != hHeap)
			&& (hTarget->pPrevHeap->dwFreeSpace < hTarget->dwFreeSpace) )
		{
			//
			// ヒープチェーンを最適化
			//
			HESLHEAP	hPrev = hTarget->pPrevHeap ;
			HESLHEAP	hNext = hTarget->pNextHeap ;
			ESLAssert( hPrev->pPrevHeap != NULL ) ;
			hTarget->pPrevHeap = hPrev->pPrevHeap ;
			hTarget->pNextHeap = hPrev ;
			hPrev->pPrevHeap->pNextHeap = hTarget ;
			hPrev->pNextHeap = hNext ;
			hPrev->pPrevHeap = hTarget ;
			if ( hNext != NULL )
			{
				hNext->pPrevHeap = hPrev ;
			}
			if ( hHeap->pLastHeap == hTarget )
			{
				ESLAssert( hNext == NULL ) ;
				ESLAssert( hPrev != hHeap ) ;
				hHeap->pLastHeap = hPrev ;
			}
		}
	}
	//
}


//////////////////////////////////////////////////////////////////////////////
// メモリの再確保
//////////////////////////////////////////////////////////////////////////////

void * eslHeapReallocate
	( HESLHEAP hHeap, void * ptrObj, DWORD dwSize, DWORD dwFlags )
{
	//
	// ポインタの有効性の検証
	//
	if ( ptrObj == NULL )
	{
		return	eslHeapAllocate( hHeap, dwSize, dwFlags ) ;
	}
	if ( ((DWORD)ptrObj) & (0x07 >> HEAP_LEN_SCALE) )
	{
		ESLTrace( "無効なポインタを解放しようとしました。\n" ) ;
		return	NULL ;
	}
	VAR_HEAP_BLOCK *	pBlock =
		(VAR_HEAP_BLOCK*) (((DWORD) ptrObj) - sizeof(VAR_HEAP_BLOCK)) ;
	if ( (pBlock->dwSize & (VAR_HEAP_FREE_FLAG | VAR_HEAP_FIRST_FLAG))
		|| !(pBlock->dwSize & VAR_HEAP_SIGNATURE) )
	{
		ESLTrace( "無効なポインタを解放しようとしました。\n" ) ;
		return	NULL ;
	}
	//
	// ヒープヘッダを取得する
	//
	if ( hHeap == NULL )
	{
		hHeap = g_hEslHeap ;
	}
	dwFlags |= hHeap->dwDefFlags ;
	//
	HESLHEAP	hTarget = NULL ;
	if ( pBlock->dwSize & VAR_HEAP_FIRST_FLAG )
	{
		hTarget = pBlock->hHeap ;
	}
	else if ( pBlock->pPrevious->dwSize & VAR_HEAP_FIRST_FLAG )
	{
		hTarget = pBlock->pPrevious->hHeap ;
	}
	else
	{
		HESLHEAP	hNextHeap = hHeap ;
		DWORD	dwBlockAddr = (DWORD) pBlock ;
		while ( hNextHeap != NULL )
		{
			DWORD	dwHeapAddr = (DWORD) hNextHeap ;
			if ( (dwHeapAddr <= dwBlockAddr)
				&& (dwBlockAddr < dwHeapAddr + hNextHeap->dwHeapSize) )
			{
				break ;
			}
			hNextHeap = hNextHeap->pNextHeap ;
		}
		ESLAssert( hNextHeap != NULL ) ;
		if ( hNextHeap == NULL )
		{
			ESLTrace( "無効なメモリブロックをヒープから解放しようとしました。\n" ) ;
			return	NULL ;
		}
		hTarget = hNextHeap ;
	}
	//
	// 再確保処理
	//
	DWORD	dwMemSize = GetBlockLength(pBlock) ;
	DWORD	dwAlignSize = ((dwSize + 0x07) & (~0x07)) ;
	if ( dwAlignSize == 0 )
	{
		dwAlignSize = 0x08 ;
	}
	if ( dwMemSize > dwAlignSize )
	{
		//
		// メモリサイズの縮小
		//
		if ( !(pBlock->dwSize & VAR_HEAP_LAST_FLAG) )
		{
			VAR_HEAP_BLOCK *	pNext = GetNextBlock( pBlock ) ;
			if ( pNext->dwSize & VAR_HEAP_FREE_FLAG )
			{
				//
				// 直後の未確保領域のサイズを拡大
				//
				VAR_FREE_BLOCK_INFO	vfbi = *(GetFreeBlockInfo(pNext)) ;
				DWORD	dwReduction =
					GetBlockLength(pBlock) - dwAlignSize ;
				pBlock->dwSize =
					(dwAlignSize >> HEAP_LEN_SCALE)
						| (pBlock->dwSize & VAR_HEAP_FLAG_MASK) ;
				//
				VAR_HEAP_BLOCK *	pNext2 = GetNextBlock(pBlock) ;
				pNext2->dwSize =
					((GetBlockLength(pNext) + dwReduction) >> HEAP_LEN_SCALE)
									| (pNext->dwSize & VAR_HEAP_FLAG_MASK) ;
				ESLAssert( pBlock == pNext->pPrevious ) ;
				pNext2->pPrevious = pBlock ;
				//
				if ( !(pNext2->dwSize & VAR_HEAP_LAST_FLAG) )
				{
					GetNextBlock(pNext2)->pPrevious = pNext2 ;
				}
				//
				*(GetFreeBlockInfo(pNext2)) = vfbi ;
//				VAR_FREE_BLOCK_INFO *	pvfbi = GetFreeBlockInfo(pNext2) ;
				if ( vfbi.pNext != NULL )
				{
					GetFreeBlockInfo(vfbi.pNext)->pPrevious = pNext2 ;
				}
				GetFreeBlockInfo(vfbi.pPrevious)->pNext = pNext2 ;
				//
				hTarget->dwFreeSpace += dwReduction ;
				dwMemSize = GetBlockLength(pBlock) ;
			}
		}
		//
		BYTE *	pbytObj = (BYTE*) GetBlockAddress( pBlock ) ;
		ptrObj = pbytObj ;
		//
		if ( dwFlags & ESL_HEAP_ZERO_INIT )
		{
			::eslFillMemory( pbytObj + dwSize, 0, dwMemSize - dwSize ) ;
		}
	}
	else if ( dwMemSize < dwAlignSize )
	{
		//
		// メモリサイズの拡大
		//
		VAR_HEAP_BLOCK *	pNext = NULL ;
		DWORD	dwLimitSize = dwMemSize ;
		//
		if ( !(pBlock->dwSize & VAR_HEAP_LAST_FLAG) )
		{
			pNext = GetNextBlock( pBlock ) ;
			if ( pNext->dwSize & VAR_HEAP_FREE_FLAG )
			{
				dwLimitSize +=
					GetBlockLength( pNext ) + sizeof(VAR_HEAP_BLOCK) ;
			}
		}
		//
		if ( dwLimitSize >= dwAlignSize )
		{
			//
			// 直後の未確保領域を確保
			//
			DWORD	dwGrowSize = dwAlignSize - dwMemSize ;
			DWORD	dwOldSize = dwMemSize ;
			//
			if ( dwGrowSize < (GetBlockLength(pNext) / 2) )
			{
				//
				// ブロックを分割して再確保
				//
				ESLAssert( !(pBlock->dwSize & VAR_HEAP_LAST_FLAG) ) ;
				VAR_HEAP_BLOCK *	pNext1 = GetNextBlock( pBlock ) ;
				VAR_FREE_BLOCK_INFO	vfbi = *(GetFreeBlockInfo(pNext1)) ;
				//
				dwMemSize = dwAlignSize ;
				pBlock->dwSize =
					(pBlock->dwSize & VAR_HEAP_FLAG_MASK)
							| (dwAlignSize >> HEAP_LEN_SCALE) ;
				//
				VAR_HEAP_BLOCK *	pNext2 = GetNextBlock( pBlock ) ;
				pNext2->dwSize =
					(pNext1->dwSize & VAR_HEAP_FLAG_MASK)
						| ((GetBlockLength(pNext1)
								- dwGrowSize) >> HEAP_LEN_SCALE) ;
				pNext2->pPrevious = pBlock ;
				//
				if ( !(pNext2->dwSize & VAR_HEAP_LAST_FLAG) )
				{
					GetNextBlock(pNext2)->pPrevious = pNext2 ;
				}
				//
				*(GetFreeBlockInfo(pNext2)) = vfbi ;
//				VAR_FREE_BLOCK_INFO *	pvfbi = GetFreeBlockInfo(pNext2) ;
				if ( vfbi.pNext != NULL )
				{
					GetFreeBlockInfo(vfbi.pNext)->pPrevious = pNext2 ;
				}
				GetFreeBlockInfo(vfbi.pPrevious)->pNext = pNext2 ;
				//
				hTarget->dwFreeSpace -= dwGrowSize ;
			}
			else
			{
				//
				// ブロックをそのまま割り当てる
				//
				ESLAssert( !(pBlock->dwSize & VAR_HEAP_LAST_FLAG) ) ;
				VAR_HEAP_BLOCK *	pNext = GetNextBlock( pBlock ) ;
				VAR_FREE_BLOCK_INFO *	pvfbi = GetFreeBlockInfo( pNext ) ;
				if ( pvfbi->pNext != NULL )
				{
					VAR_FREE_BLOCK_INFO *
						pvfbiNext = GetFreeBlockInfo( pvfbi->pNext ) ;
					pvfbiNext->pPrevious = pvfbi->pPrevious ;
				}
				GetFreeBlockInfo(pvfbi->pPrevious)->pNext = pvfbi->pNext ;
				//
				dwGrowSize = sizeof(VAR_HEAP_BLOCK) + GetBlockLength(pNext) ;
				dwMemSize += dwGrowSize ;
				pBlock->dwSize =
					(pBlock->dwSize & VAR_HEAP_FLAG_MASK)
								| (dwMemSize >> HEAP_LEN_SCALE) ;
				//
				if ( pNext->dwSize & VAR_HEAP_LAST_FLAG )
				{
					pBlock->dwSize |= VAR_HEAP_LAST_FLAG ;
				}
				else
				{
					ESLAssert( GetNextBlock(pBlock) == GetNextBlock(pNext) ) ;
					GetNextBlock(pBlock)->pPrevious = pBlock ;
				}
				//
				hTarget->dwFreeSpace -= dwGrowSize - sizeof(VAR_HEAP_BLOCK) ;
			}
			//
			BYTE *	pbytObj = (BYTE*) GetBlockAddress( pBlock ) ;
			ptrObj = pbytObj ;
			//
			if ( dwFlags & ESL_HEAP_ZERO_INIT )
			{
				::eslFillMemory( pbytObj + dwOldSize, 0, dwMemSize - dwOldSize ) ;
			}
		}
		else
		{
			//
			// 別の領域にメモリを再確保
			//
			BYTE *	ptrNew =
				(BYTE*) ::eslHeapAllocate
					( hHeap, dwSize, dwFlags | ESL_HEAP_NO_SERIALIZE ) ;
			BYTE *	pbytOld = (BYTE*) ptrObj ;
			ptrObj = ptrNew ;
			//
			::eslMoveMemory( ptrNew, pbytOld, dwMemSize ) ;
			//
			::eslHeapFree( hHeap, pbytOld, ESL_HEAP_NO_SERIALIZE ) ;
		}
	}
	else
	{
	}
	//
	//
	return	ptrObj ;
}


//////////////////////////////////////////////////////////////////////////////
// メモリサイズの取得（メモリの有効性の検証）
//////////////////////////////////////////////////////////////////////////////

DWORD eslHeapGetLength( HESLHEAP hHeap, void * ptrObj )
{
	try
	{
		//
		// ポインタの有効性の検証
		//
		if ( (ptrObj == NULL)
			|| (((DWORD)ptrObj) & (0x07 >> HEAP_LEN_SCALE)) )
		{
			return	ESL_INVALID_HEAP ;
		}
		VAR_HEAP_BLOCK *	pBlock =
			(VAR_HEAP_BLOCK*) (((DWORD) ptrObj) - sizeof(VAR_HEAP_BLOCK)) ;
		if ( (pBlock->dwSize & VAR_HEAP_FREE_FLAG)
			|| (pBlock->dwSize & VAR_HEAP_FIRST_FLAG)
			|| !(pBlock->dwSize & VAR_HEAP_SIGNATURE) )
		{
			return	ESL_INVALID_HEAP ;
		}
		//
		// サイズの取得
		//
		DWORD	dwSize = pBlock->dwSize ;
		if ( (dwSize & VAR_HEAP_FLAG_MASK) ==
			(VAR_HEAP_SIGNATURE | VAR_HEAP_LAST_FLAG | VAR_HEAP_FIRST_FLAG) )
		{
			dwSize = GetBlockLength( pBlock ) - 8 ;
		}
		else
		{
			dwSize = GetBlockLength( pBlock ) ;
		}
		return	dwSize ;
	}
	catch ( ... )
	{
		//
		// ポインタは無効
		//
		return	ESL_INVALID_HEAP ;
	}
}


//////////////////////////////////////////////////////////////////////////////
// ヒープに対する他のスレッドからの操作を待機
//////////////////////////////////////////////////////////////////////////////

void eslHeapLock( HESLHEAP hHeap )
{
	if ( hHeap == NULL )
	{
		hHeap = ::eslGetGlobalHeap( ) ;
	}
}


//////////////////////////////////////////////////////////////////////////////
// ヒープに対する他のスレッドからの操作の待機を解除
//////////////////////////////////////////////////////////////////////////////

void eslHeapUnlock( HESLHEAP hHeap )
{
	if ( hHeap == NULL )
	{
		hHeap = g_hEslHeap ;
	}
}


//////////////////////////////////////////////////////////////////////////////
// ヒープに確保されているメモリブロックをデバッグ出力へダンプ
//////////////////////////////////////////////////////////////////////////////

void eslFillMemory(void *ptrMem, BYTE bytData, DWORD dwLength)
{
    memset(ptrMem,bytData,dwLength);
}

void eslMoveMemory(void *ptrDst, const void *ptrSrc, DWORD dwLength)
{
    memcpy(ptrDst,ptrSrc,dwLength);
}
