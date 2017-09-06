
/*****************************************************************************
                         E R I S A - L i b r a r y
 -----------------------------------------------------------------------------
    Copyright (C) 2002-2004 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/


#if	!defined(__ERISALIB_H__)
#define	__ERISALIB_H__

//
// 必要なデータ型やテキストマクロを定義します
//////////////////////////////////////////////////////////////////////////////
// コンパイルに必要なデータ型；
//		BYTE	: 符号無し8ビット整数
//		WORD	: 符号無し16ビット整数
//		SWORD	: 符号あり16ビット整数
//		DWORD	: 符号無し32ビット整数
//		SDWORD	: 符号有り32ビット整数
//		INT		: 符号有り16ビット以上整数
//		UINT	: 符号無し16ビット以上整数
//		LONG	: 符号有り32ビット以上整数
//		ULONG	: 符号無し32ビット以上整数
//		INT64	: 符号有り64ビット整数
//		UINT64	: 符号無し64ビット整数
//		PVOID	: void 型へのポインタ
//		PBYTE	: 符号無し8ビット整数へのポインタ
//		PWORD	: 符号無し16ビット整数へのポインタ
//		PINT	: 符号有り16ビット以上整数へのポインタ
//		REAL32	: 32ビット浮動小数点
//		REAL64	: 64ビット浮動小数点


typedef	void *					PVOID ;
typedef	unsigned char			BYTE, * PBYTE ;
typedef	unsigned short int		WORD, * PWORD ;
typedef	unsigned long int		DWORD ;
typedef	signed int				INT, * PINT ;
typedef	unsigned int			UINT ;
typedef	signed long int			LONG, * PLONG ;
typedef	unsigned long int		ULONG ;

typedef	signed char				SBYTE ;
typedef	signed short int		SWORD ;
typedef	signed long int			SDWORD ;
typedef	signed long long		INT64 ;
typedef	unsigned long long		UINT64 ;
typedef	float					REAL32 ;
typedef	double					REAL64 ;

#include "esl.h"

/*****************************************************************************
                       ライブラリ初期化・終了関数
 *****************************************************************************/

#if	defined(_M_IX86) && !defined(ERI_INTEL_X86)
#define	ERI_INTEL_X86
#endif

void eriInitializeLibrary( void ) ;
void eriInitializeCodec( void ) ;

/*****************************************************************************
                                画像情報
 *****************************************************************************/

struct	ERI_FILE_HEADER
{
	DWORD	dwVersion ;
	DWORD	dwContainedFlag ;
	DWORD	dwKeyFrameCount ;
	DWORD	dwFrameCount ;
	DWORD	dwAllFrameTime ;
} ;

struct	ERI_INFO_HEADER
{
	DWORD	dwVersion ;
	DWORD	fdwTransformation ;
	DWORD	dwArchitecture ;
	DWORD	fdwFormatType ;
	SDWORD	nImageWidth ;
	SDWORD	nImageHeight ;
	DWORD	dwBitsPerPixel ;
	DWORD	dwClippedPixel ;
	DWORD	dwSamplingFlags ;
	SDWORD	dwQuantumizedBits[2] ;
	DWORD	dwAllottedBits[2] ;
	DWORD	dwBlockingDegree ;
	DWORD	dwLappedBlock ;
	DWORD	dwFrameTransform ;
	DWORD	dwFrameDegree ;
} ;

#define	EFH_STANDARD_VERSION	0x00020100
#define	EFH_ENHANCED_VERSION	0x00020200

#define	EFH_CONTAIN_IMAGE		0x00000001
#define	EFH_CONTAIN_ALPHA		0x00000002
#define	EFH_CONTAIN_PALETTE		0x00000010
#define	EFH_CONTAIN_WAVE		0x00000100
#define	EFH_CONTAIN_SEQUENCE	0x00000200

#define	ERI_RGB_IMAGE			0x00000001
#define	ERI_RGBA_IMAGE			0x04000001
#define	ERI_GRAY_IMAGE			0x00000002
#define	ERI_TYPE_MASK			0x00FFFFFF
#define	ERI_WITH_PALETTE		0x01000000
#define	ERI_USE_CLIPPING		0x02000000
#define	ERI_WITH_ALPHA			0x04000000

#define	CVTYPE_LOSSLESS_ERI		0x03020000
#define	CVTYPE_DCT_ERI			0x00000001
#define	CVTYPE_LOT_ERI			0x00000005
#define	CVTYPE_LOT_ERI_MSS		0x00000105

#define	ERI_ARITHMETIC_CODE		32
#define	ERI_RUNLENGTH_GAMMA		0xFFFFFFFF
#define	ERI_RUNLENGTH_HUFFMAN	0xFFFFFFFC
#define	ERISA_NEMESIS_CODE		0xFFFFFFF0

#define	ERISF_YUV_4_4_4			0x00040404
#define	ERISF_YUV_4_2_2			0x00040202
#define	ERISF_YUV_4_1_1			0x00040101


/*****************************************************************************
                                音声情報
 *****************************************************************************/

struct	MIO_INFO_HEADER
{
	DWORD	dwVersion ;
	DWORD	fdwTransformation ;
	DWORD	dwArchitecture ;
	DWORD	dwChannelCount ;
	DWORD	dwSamplesPerSec ;
	DWORD	dwBlocksetCount ;
	DWORD	dwSubbandDegree ;
	DWORD	dwAllSampleCount ;
	DWORD	dwLappedDegree ;
	DWORD	dwBitsPerSample ;
} ;

struct	MIO_DATA_HEADER
{
	BYTE	bytVersion ;
	BYTE	bytFlags ;
	BYTE	bytReserved1 ;
	BYTE	bytReserved2 ;
	DWORD	dwSampleCount ;
} ;

#define	MIO_LEAD_BLOCK	0x01


/*****************************************************************************
                            Функции для анимации
 *****************************************************************************/

#include "egl2d.h"

ESLError eriCopyImage ( const EGL_IMAGE_INFO & eiiDst, const EGL_IMAGE_INFO & eiiSrc ) ;
ESLError eriBlendHalfImage ( const EGL_IMAGE_INFO & eiiDst, const EGL_IMAGE_INFO & eiiSrc1, const EGL_IMAGE_INFO & eiiSrc2 ) ;
ESLError eriLLSubtractionOfFrame ( const EGL_IMAGE_INFO & eiiDst, const EGL_IMAGE_INFO & eiiSrc ) ;
int eriLSSubtractionOfFrame ( const EGL_IMAGE_INFO & eiiDst, const EGL_IMAGE_INFO & eiiSrc ) ;
DWORD eriSumAbsDifferenceOfBlock ( const EGL_IMAGE_INFO & eiiDst, const EGL_IMAGE_INFO & eiiSrc ) ;
void eriImageFilterLoop421 ( const EGL_IMAGE_INFO & eiiDst, const EGL_IMAGE_INFO * eiiSrc = NULL, SBYTE * pFlags = NULL, int nBlockSize = 16 ) ;


class	ERISADecodeContext ;
class	ERISAEncodeContext ;

class	ERISADecoder ;
class	ERISAEncoder ;

class	MIOEncoder ;
class	MIODecoder ;

class	EMCFile ;
class	ERIFile ;

class	MIODynamicPlayer ;
class	ERIAnimation ;
class	ERIAnimationWriter ;


#include "erisamatrix.h"
#include "erisacontext.h"
#include "erisaimage.h"
#include "erisasound.h"
#include "erisafile.h"
#include "erisaplay.h"
#include "math.h"


#endif
