
/*****************************************************************************
                    Entis Standard Library declarations
 ----------------------------------------------------------------------------
        Copyright (c) 2002-2003 Leshade Entis. All rights reserved.
 *****************************************************************************/


#if	!defined(__ESL_H__)
#define	__ESL_H__


//////////////////////////////////////////////////////////////////////////////
// デバッグサポート関数
//////////////////////////////////////////////////////////////////////////////


#include <stdio.h>
inline void ESLTrace( const char * pszTrace, ... )
{
    printf(pszTrace);
}
#define	ESLAssert(expr)	((void)0)


//////////////////////////////////////////////////////////////////////////////
// 共通エラーコード
//////////////////////////////////////////////////////////////////////////////

enum	ESLError
{
	eslErrSuccess		= 0,
	eslErrNotSupported	= -1,
	eslErrGeneral		= 1,
	eslErrAbort			= 2,
	eslErrInvalidParam	= 3,
	eslErrTimeout		= 4,
	eslErrPending		= 5,
	eslErrContinue		= 6,
	eslErrDummy			= 0xFFFFFFFF
} ;

inline ESLError ESLErrorMsg( const char * pszMsg )
{
    printf(pszMsg);
    printf("\n");
	return	(ESLError) (long int) pszMsg ;
}

const char * GetESLErrorMsg( ESLError err ) ;


//////////////////////////////////////////////////////////////////////////////
// ヒープ関数
//////////////////////////////////////////////////////////////////////////////

#include	"eslheap.h"


//////////////////////////////////////////////////////////////////////////////
// 基底クラス
//////////////////////////////////////////////////////////////////////////////

class	ESLObject
{
public:
	static const char *const	m_pszClassName ;
	ESLObject( void ) { }
	virtual ~ESLObject( void ) { }
	static void * operator new ( size_t stObj ) ;
	static void * operator new ( size_t stObj, void * ptrObj ) ;
	static void * operator new ( size_t stObj, const char * pszFileName, int nLine ) ;
	static void operator delete ( void * ptrObj ) ;

} ;

//////////////////////////////////////////////////////////////////////////////
// Entis Library 低水準クラスライブラリ
//////////////////////////////////////////////////////////////////////////////

template <class _Type, class _Obj> class	EGenString ;
class	EString ;
class	EWideString ;
class	EStreamWideString ;

class	EPtrBuffer ;
class	EStreamBuffer ;

class	EPtrArray ;
template <class> class	ENumArray ;
template <class> class	EPtrObjArray ;
template <class> class	EObjArray ;
template <class TagType, class ObjType> class	ETagSortArray ;
template <class>	class	EIntTagArray ;
template <class>	class	EStrTagArray ;
template <class>	class	EWStrTagArray ;
template <class TagType, class ObjType> class	ETaggedElement ;

class	ESLFileObject ;
class	EMemoryFile ;
class	EStreamFileBuffer ;

#include	"eslarray.h"
#include	"eslstring.h"
#include	"eslfile.h"


#endif

