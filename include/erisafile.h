
/*****************************************************************************
                         E R I S A - L i b r a r y
 -----------------------------------------------------------------------------
    Copyright (C) 2002-2003 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/


#if	!defined(__ERISA_FILE_H__)
#define	__ERISA_FILE_H__	1
#include "eslstring.h"


/*****************************************************************************
                  EMC (Entis Media Complex) ファイル形式
 *****************************************************************************/

class	EMCFile	: public	ESLFileObject
{
public:
	// 構築関数
	EMCFile( void ) ;
	// 消滅関数
	virtual ~EMCFile( void ) ;
	// クラス情報
    //DECLARE_CLASS_INFO( EMCFile, ESLFileObject )

public:
	enum	FileIdentity
	{
		fidArchive			= 0x02000400,
		fidRasterizedImage	= 0x03000100,
		fidEGL3DSurface		= 0x03001100,
		fidEGL3DModel		= 0x03001200,
		fidUndefinedEMC		= -1
	} ;
	struct	FILE_HEADER
	{
		char	cHeader[8] ;			// ファイルシグネチャ
		DWORD	dwFileID ;				// ファイル識別子
		DWORD	dwReserved ;			// 予約＝０
		char	cFormatDesc[0x30] ;		// フォーマット名
	} ;
protected:
	struct	RECORD_HEADER
	{
		UINT64			nRecordID ;		// レコードの識別子
		UINT64			nRecLength ;	// レコードの長さ
	} ;
	struct	RECORD_INFO
	{
		RECORD_INFO *	pParent ;		// 親レコード
		DWORD			dwWriteFlag ;	// 書き込みフラグ
		UINT64			qwBasePos ;		// レコードの基準位置
		RECORD_HEADER	rechdr ;		// レコードヘッダ
	} ;

	ESLFileObject *	m_pFile ;			// ファイルオブジェクト
	RECORD_INFO *	m_pRecord ;			// 現在のレコード
	FILE_HEADER		m_fhHeader ;		// ファイルヘッダ

	static char	m_cDefSignature[8] ;

public:
	// ファイルを開く
	ESLError Open( ESLFileObject * pfile,
				const FILE_HEADER * pfhHeader = NULL ) ;
	// ファイルを閉じる
	void Close( void ) ;
	// EMC ファイルを複製
	virtual ESLFileObject * Duplicate( void ) const ;

public:
	// ファイルヘッダ取得
	const FILE_HEADER & GetFileHeader( void ) const
		{
			return	m_fhHeader ;
		}
	// ファイルヘッダ初期化
	static void SetFileHeader
		( FILE_HEADER & fhHeader,
			DWORD dwFileID, const char * pszDesc = NULL ) ;
	// デフォルトファイルシグネチャを取得
	static void GetFileSignature( char cHeader[8] ) ;
	// デフォルトファイルシグネチャを設定
	static void SetFileSignature( const char cHeader[8] ) ;

public:
	// レコードを開く
	virtual ESLError DescendRecord( const UINT64 * pRecID = NULL ) ;
	// レコードを閉じる
	virtual ESLError AscendRecord( void ) ;
	// レコード識別子を取得
	UINT64 GetRecordID( void ) const
		{
			ESLAssert( m_pRecord != NULL ) ;
			return	m_pRecord->rechdr.nRecordID ;
		}
	// レコード長を取得
	UINT64 GetRecordLength( void ) const
		{
			ESLAssert( m_pRecord != NULL ) ;
			return	m_pRecord->rechdr.nRecLength ;
		}

public:
	// データを読み込む
	virtual unsigned long int Read
		( void * ptrBuffer, unsigned long int nBytes ) ;
	// データを書き出す
	virtual unsigned long int Write
		( const void * ptrBuffer, unsigned long int nBytes ) ;
	// レコードの長さを取得する
	virtual UINT64 GetLargeLength( void ) const ;
	virtual unsigned long int GetLength( void ) const ;
	// ファイルポインタを移動する
	virtual UINT64 SeekLarge
		( INT64 nOffsetPos, SeekOrigin fSeekFrom ) ;
	virtual unsigned long int Seek
		( long int nOffsetPos, SeekOrigin fSeekFrom ) ;
	// ファイルポインタを取得する
	virtual UINT64 GetLargePosition( void ) const ;
	virtual unsigned long int GetPosition( void ) const ;
	// ファイルの終端を現在の位置に設定する
	virtual ESLError SetEndOfFile( void ) ;

} ;


/*****************************************************************************
                        ERI ファイルインターフェース
 *****************************************************************************/

class	ERIFile	: public	EMCFile
{
public:
	//
	// タグ情報インデックス
	enum	TagIndex
	{
		tagTitle,				// 曲名
		tagVocalPlayer,			// 歌手・演奏者
		tagComposer,			// 作曲者
		tagArranger,			// 編曲者
		tagSource,				// 出展・アルバム
		tagTrack,				// トラック
		tagReleaseDate,			// リリース年月日
		tagGenre,				// ジャンル
		tagRewindPoint,			// ループポイント
		tagHotSpot,				// ホットスポット
		tagResolution,			// 解像度
		tagComment,				// コメント
		tagWords,				// 歌詞
		tagMax
	} ;
	//
	// タグ情報文字列
	static const wchar_t *	m_pwszTagName[tagMax] ;
	//
	// タグ情報オブジェクト
	class	ETagObject
	{
	public:
		EWideString		m_tag ;
		EWideString		m_contents ;
	public:
		// 構築関数
		ETagObject( void ) ;
		// 消滅関数
		~ETagObject( void ) ;
	} ;
	//
	// タグ情報解析オブジェクト
	class	ETagInfo
	{
	public:
		EObjArray<ETagObject>	m_lstTags ;
	public:
		// 構築関数
		ETagInfo( void ) ;
		// 消滅関数
		~ETagInfo( void ) ;
		// タグ情報解析
		void CreateTagInfo( const wchar_t * pwszDesc ) ;
		// タグ情報をフォーマット
		void FormatDescription( EWideString & wstrDesc ) ;
		// タグを追加する
		void AddTag( TagIndex tagIndex, const wchar_t * pwszContents ) ;
		// タグ情報のクリア
		void DeleteContents( void ) ;
		// タグ情報取得
		const wchar_t * GetTagContents( const wchar_t * pwszTag ) const ;
		const wchar_t * GetTagContents( TagIndex tagIndex ) const ;
	} ;
	//
	// シーケンス構造体
	struct	SEQUENCE_DELTA
	{
		DWORD	dwFrameIndex ;
		DWORD	dwDuration ;
	} ;

	// 読み込みマスク
	enum	ReadMask
	{
		rmFileHeader	= 0x00000001,
		rmPreviewInfo	= 0x00000002,
		rmImageInfo		= 0x00000004,
		rmSoundInfo		= 0x00000008,
		rmCopyright		= 0x00000010,
		rmDescription	= 0x00000020,
		rmPaletteTable	= 0x00000040,
		rmSequenceTable	= 0x00000080
	} ;
	DWORD			m_fdwReadMask ;
	// ファイルヘッダ
	ERI_FILE_HEADER	m_FileHeader ;
	// プレビュー画像情報ヘッダ
	ERI_INFO_HEADER	m_PreviewInfo ;
	// 画像情報ヘッダ
	ERI_INFO_HEADER	m_InfoHeader ;
	// 音声情報ヘッダ
	MIO_INFO_HEADER	m_MIOInfHdr ;
	// パレットテーブル
	EGL_PALETTE		m_PaletteTable[0x100] ;

protected:
	DWORD				m_dwSeqLength ;
	SEQUENCE_DELTA *	m_pSequence ;

public:
	// 構築関数
	ERIFile( void ) ;
	// 消滅関数
	virtual ~ERIFile( void ) ;
	// クラス情報
    //DECLARE_CLASS_INFO( ERIFile, EMCFile )

public:
	// ファイルのオープン方法
	enum	OpenType
	{
		otOpenRoot,			// ルートレコードを開くだけ
		otReadHeader,		// 情報ヘッダレコードを読み込んで値を検証
		otOpenStream,		// ヘッダを読み込みストリームレコードを開く
		otOpenImageData		// 画像データレコードを開く
	} ;
	// ERI ファイルを開く
	ESLError Open( ESLFileObject * pFile, OpenType type = otOpenImageData ) ;

public:
	// シーケンステーブルを取得する
	const SEQUENCE_DELTA * GetSequenceTable( DWORD * pdwLength ) const ;

} ;



#endif
