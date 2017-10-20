#ifndef __CTP_SESSION_H__
#define __CTP_SESSION_H__


#pragma warning(disable:4786)
#include <set>
#include <string>
#include <stdexcept>
#include "DataDump.h"
#include "ThostFtdcMdApi.h"
#include "../Configuration.h"
#include "../CTP_DL_QuoProtocal.h"
#include "../Infrastructure/Lock.h"
#include "../Infrastructure/Thread.h"




/**
 * @class							PackagesLoopBuffer
 * @brief							���ݰ����л���
 * @detail							struct PkgHead + MessageID1 + data block1 + data block2 + struct PkgHead + MessageID2 + data block1 + data block2 + ...
 * @author							barry
 */
class PackagesLoopBuffer
{
public:
	PackagesLoopBuffer();
	~PackagesLoopBuffer();

public:
	/**
	 * @brief						��ʼ���������
	 * @param[in]					nMaxBufSize				������Ļ����С
	 * @return						==0						�ɹ�
	 */
	int								Initialize( unsigned long nMaxBufSize );

	/**
	 * @brief						�ͷŻ���ռ�
	 */
	void							Release();

public:
	/**
	 * @brief						�洢����
	 * @param[in]					nDataID					����ID
	 * @param[in]					pData					����ָ��
	 * @param[in]					nDataSize				���ݳ���
	 * @param[in]					nSeqNo					��ǰ���ݿ�ĸ������
	 * @return						==0						�ɹ�
	 * @note						��nDataID������ǰһ������nDataIDʱ����������һ��Package��װ
	 */
	int								PushBlock( unsigned int nDataID, const char* pData, unsigned int nDataSize, unsigned __int64 nSeqNo, unsigned int& nMsgCount, unsigned int& nBodySize );

	/**
	 * @brief						��ȡһ�����ݰ�
	 * @param[out]					pBuff					������ݻ����ַ
	 * @param[in]					nBuffSize				���ݻ��泤��
	 * @param[out]					nMsgID					������ϢID
	 * @return						>0						���ݳ���
									==0						������
									<0						����
	 */
	int								GetOnePkg( char* pBuff, unsigned int nBuffSize, unsigned int& nMsgID );

	/**
	 * @brief						�Ƿ�Ϊ��
	 * @return						true					Ϊ��
	 */
	bool							IsEmpty();

	/**
	 * @brief						��ȡ���¿ռ�İٷֱ�
	 */
	float							GetPercentOfFreeSize();

protected:
	CriticalObject					m_oLock;				///< ��
	char*							m_pPkgBuffer;			///< ���ݰ������ַ
	unsigned int					m_nMaxPkgBufSize;		///< ���ݰ������С
	unsigned int					m_nCurrentPkgHeadPos;	///< ��ǰ��ʼλ��(��ǰ��д����İ�ͷ)
	unsigned int					m_nFirstPkgHeadPos;		///< ��ʼλ������(��д�����Ѿ�д����İ�ͷ����δ�������ݵ�ͷ��)
	unsigned int					m_nCurrentWritePos;		///< ����λ������(����д���λ��)
};



/**
 * @class			CTPWorkStatus
 * @brief			CTP����״̬����
 * @author			barry
 */
class CTPWorkStatus
{
public:
	/**
	 * @brief				Ӧ״ֵ̬ӳ���״̬�ַ���
	 */
	static	std::string&	CastStatusStr( enum E_SS_Status eStatus );

public:
	/**
	 * @brief			����
	 * @param			eMkID			�г����
	 */
	CTPWorkStatus();
	CTPWorkStatus( const CTPWorkStatus& refStatus );

	/**
	 * @brief			��ֵ����
						ÿ��ֵ�仯������¼��־
	 */
	CTPWorkStatus&		operator= ( enum E_SS_Status eWorkStatus );

	/**
	 * @brief			����ת����
	 */
	operator			enum E_SS_Status();

private:
	CriticalObject		m_oLock;				///< �ٽ�������
	enum E_SS_Status	m_eWorkStatus;			///< ���鹤��״̬
};


typedef	std::set<std::string>		T_SET_INSTRUMENTID;			///< ��Ʒ���뼯��


/**
 * @class			CTPQuotation
 * @brief			CTP�Ự�������
 * @detail			��װ�������Ʒ�ڻ���Ȩ���г��ĳ�ʼ����������Ƶȷ���ķ���
 * @author			barry
 */
class CTPQuotation : public CThostFtdcMdSpi, public SimpleTask
{
public:
	CTPQuotation();
	~CTPQuotation();

	/**
	 * @brief			�ͷ�ctp����ӿ�
	 */
	int					Destroy();

	/**
	 * @brief			��ʼ��ctp����ӿ�
	 * @return			>=0			�ɹ�
						<0			����
	 * @note			������������������У�ֻ������ʱ��ʵ�ĵ���һ��
	 */
	int					Activate() throw(std::runtime_error);

protected:
	/**
	 * @brief			�����µ�������¶�������
	 * @return			>=0			�ɹ�
	 */
	int					SubscribeQuotation();

	/**
	 * @brief			������(��ѭ��)
	 * @return			==0					�ɹ�
						!=0					ʧ��
	 */
	int					Execute();

public:///< ������������
	/**
	 * @brief			������������
	 * @param[in]		sFilePath			�ļ�·��
	 * @param[in]		bEchoOnly			���Ա�ʶ
	 * @return			>=0					�ɹ�
	 */
	int					LoadDataFile( std::string sFilePath, bool bEchoOnly = false );

	/**
	 * @brief			��ȡ�Ự״̬��Ϣ
	 */
	CTPWorkStatus&		GetWorkStatus();

	/**
	 * @brief			��ȡ��������
	 */
	unsigned int		GetCodeCount();

	/**
	 * @brief			���͵�¼�����
	 */
    void				SendLoginRequest();

	/** 
	 * @brief			ˢ���ݵ����ͻ���
	 * @param[in]		pQuotationData			�������ݽṹ
	 * @param[in]		bInitialize				��ʼ�����ݵı�ʶ
	 */
	void				FlushQuotation( CThostFtdcDepthMarketDataField* pQuotationData, bool bInitialize );

public:///< CThostFtdcMdSpi�Ļص��ӿ�
	void OnFrontConnected();
	void OnFrontDisconnected( int nReason );
	void OnHeartBeatWarning( int nTimeLapse );
	void OnRspUserLogin( CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );
	void OnRtnDepthMarketData( CThostFtdcDepthMarketDataField *pMarketData );
	void OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );

private:
	CThostFtdcMdApi*	m_pCTPApi;				///< �������ṩ��cffex�ӿ�
	CTPWorkStatus		m_oWorkStatus;			///< ����״̬
	T_SET_INSTRUMENTID	m_setRecvCode;			///< �յ��Ĵ��뼯��
	unsigned int		m_nCodeCount;			///< �յ��Ŀ�����Ʒ����
	CriticalObject		m_oLock;				///< �ٽ�������
protected:
	QuotationRecorder	m_oDataRecorder;		///< �������̶���
};




#endif






