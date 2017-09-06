﻿
/*****************************************************************************
                   Entis Standard Library declarations
 ----------------------------------------------------------------------------

	In this file, the file object classes definitions.

	Copyright (C) 1998-2002 Leshade Entis.  All rights reserved.

 ****************************************************************************/


#if	!defined(__ESLFILE_H__)
#define	__ESLFILE_H__	1

typedef void* HANDLE;

/*****************************************************************************
                           ファイル抽象クラス
 ****************************************************************************/

class	ESLFileObject : public	ESLObject
{
public:
	// 構築関数
	ESLFileObject( void ) ;
	// 消滅関数
	virtual ~ESLFileObject( void ) ;

protected:
    //CRITICAL_SECTION	m_cs ;
	int					m_nAttribute ;

public:
	// スレッド排他アクセス用関数
	void Lock( void ) const ;
	void Unlock( void ) const ;

public:
	// 属性
	enum	OpenFlag
	{
		modeCreateFlag	= 0x0001 ,
		modeCreate		= 0x0005 ,
		modeRead		= 0x0002 ,
		modeWrite		= 0x0004 ,
		modeReadWrite	= 0x0006 ,
		shareRead		= 0x0010 ,
		shareWrite		= 0x0020
	} ;
protected:
	void SetAttribute( int nAttribute )
		{
			m_nAttribute = nAttribute ;
		}
public:
	int GetAttribute( void ) const
		{
			return	m_nAttribute ;
		}

public:
	// ファイルオブジェクトを複製する
	virtual ESLFileObject * Duplicate( void ) const = 0 ;

public:
	// ファイルから読み込む
	virtual unsigned long int Read
		( void * ptrBuffer, unsigned long int nBytes ) = 0 ;
	// ファイルへ書き出す
	virtual unsigned long int Write
		( const void * ptrBuffer, unsigned long int nBytes ) = 0 ;

public:
	// シーク方法
	enum	SeekOrigin
	{
        FromBegin	= 0,
        FromCurrent	= 1,
        FromEnd		= 2
	} ;
	// ファイルの長さを取得
	virtual unsigned long int GetLength( void ) const = 0 ;
	// ファイルポインタを移動
	virtual unsigned long int Seek
		( long int nOffsetPos, SeekOrigin fSeekFrom ) = 0 ;
	// ファイルポインタを取得
	virtual unsigned long int GetPosition( void ) const = 0 ;
	// ファイルの終端を現在の位置に設定する
	virtual ESLError SetEndOfFile( void ) ;

public:	// 64 ビットファイル長
	// ファイルの長さを取得
	virtual UINT64 GetLargeLength( void ) const ;
	// ファイルポインタを移動
	virtual UINT64 SeekLarge
		( INT64 nOffsetPos, SeekOrigin fSeekFrom ) ;
	// ファイルポインタを取得
	virtual UINT64 GetLargePosition( void ) const ;

} ;


/*****************************************************************************
                         メモリファイルクラス
 ****************************************************************************/

class	EMemoryFile : public	ESLFileObject
{
protected:
	void *				m_ptrMemory ;
	unsigned long int	m_nLength ;
	unsigned long int	m_nPosition ;
	unsigned long int	m_nBufferSize ;

public:
	// 構築関数
	EMemoryFile( void ) ;
	// 消滅関数
	virtual ~EMemoryFile( void ) ;
	// クラス情報
    //DECLARE_CLASS_INFO( EMemoryFile, ESLFileObject )

public:
	// 読み書き可能なメモリファイルを作成する
	ESLError Create( unsigned long int nLength ) ;
	// 読み込み専用のメモリファイルを作成する
	ESLError Open( const void * ptrMemory, unsigned long int nLength ) ;
	// 内部リソースを解放する
	void Delete( void ) ;
	// 内部メモリへのポインタを取得する
	void * GetBuffer( void ) const
		{
			return	m_ptrMemory ;
		}

	// メモリファイルを複製する
	virtual ESLFileObject * Duplicate( void ) const ;

public:
	// メモリファイルからデータを転送する
	virtual unsigned long int Read
		( void * ptrBuffer, unsigned long int nBytes ) ;
	// メモリファイルへデータを転送する
	virtual unsigned long int Write
		( const void * ptrBuffer, unsigned long int nBytes ) ;

public:
	// メモリファイルの長さを取得する
	virtual unsigned long int GetLength( void ) const ;
	// ファイルポインタを移動する
	virtual unsigned long int Seek
		( long int nOffsetPos, SeekOrigin fSeekFrom ) ;
	// ファイルポインタを取得する
	virtual unsigned long int GetPosition( void ) const ;
	// ファイルの終端を現在の位置に設定する
	virtual ESLError SetEndOfFile( void ) ;

} ;


/*****************************************************************************
                 ストリーミングバッファファイルクラス
 ****************************************************************************/

class	EStreamFileBuffer	: public ESLFileObject, public EStreamBuffer
{
public:
	// 構築関数
	EStreamFileBuffer( void ) ;
	// 消滅関数
	virtual ~EStreamFileBuffer( void ) ;
	// クラス情報
    //DECLARE_CLASS_INFO2( EStreamFileBuffer, ESLFileObject, EStreamBuffer )

public:
	// ファイルオブジェクトを複製する
	virtual ESLFileObject * Duplicate( void ) const ;

public:
	// ファイルから読み込む
	virtual unsigned long int Read
		( void * ptrBuffer, unsigned long int nBytes ) ;
	// ファイルへ書き出す
	virtual unsigned long int Write
		( const void * ptrBuffer, unsigned long int nBytes ) ;

public:
	// ファイルの長さを取得
	virtual unsigned long int GetLength( void ) const ;
	// ファイルポインタを移動
	virtual unsigned long int Seek
		( long int nOffsetPos, SeekOrigin fSeekFrom ) ;
	// ファイルポインタを取得
	virtual unsigned long int GetPosition( void ) const ;

} ;


#endif
