#include "CTPQuoImage.h"
#include "../DataCollector4CTPDL.h"
#pragma comment(lib, "./CTPConnection/thosttraderapi.lib")


CTPQuoImage::CTPQuoImage()
 : m_pTraderApi( NULL ), m_nTrdReqID( 0 ), m_bIsResponded( false )
{
}

CTPQuoImage::operator T_MAP_BASEDATA&()
{
	CriticalLock	section( m_oLock );

	return m_mapBasicData;
}

int CTPQuoImage::GetSubscribeCodeList( char (&pszCodeList)[1024*5][20], unsigned int nListSize )
{
	unsigned int	nRet = 0;
	CriticalLock	section( m_oLock );

	for( T_MAP_BASEDATA::iterator it = m_mapBasicData.begin(); it != m_mapBasicData.end() && nRet < nListSize; it++ ) {
		::strncpy( pszCodeList[nRet++], it->second.InstrumentID, 20 );
	}

	return nRet;
}

int CTPQuoImage::FreshCache()
{
	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::FreshCache() : ............ freshing basic data ..............." );

	FreeApi();///< ���������� && ����api���ƶ���
	if( NULL == (m_pTraderApi=CThostFtdcTraderApi::CreateFtdcTraderApi()) )
	{
		QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::FreshCache() : error occur while creating CTP trade control api" );
		return -1;
	}

	char			pszTmpFile[128] = { 0 };						///< ׼������ľ�̬��������
	unsigned int	nNowTime = DateTime::Now().TimeToLong();
	if( nNowTime > 80000 && nNowTime < 110000 )
		::strcpy( pszTmpFile, "Trade_am.dmp" );
	else
		::strcpy( pszTmpFile, "Trade_pm.dmp" );
	std::string		sDumpFile = GenFilePathByWeek( Configuration::GetConfig().GetDumpFolder().c_str(), pszTmpFile, DateTime::Now().DateToLong() );
	QuotationSync::CTPSyncSaver::GetHandle().Init( sDumpFile.c_str(), DateTime::Now().DateToLong(), true );

	m_pTraderApi->RegisterSpi( this );								///< ��thisע��Ϊ�¼������ʵ��
	if( false == Configuration::GetConfig().GetTrdConfList().RegisterServer( NULL, m_pTraderApi ) )///< ע��CTP������Ҫ����������
	{
		QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::FreshCache() : invalid front/name server address" );
		return -2;
	}

	m_pTraderApi->Init();											///< ʹ�ͻ��˿�ʼ�����鷢����������������
	for( int nLoop = 0; false == m_bIsResponded; nLoop++ )			///< �ȴ�������Ӧ����
	{
		SimpleThread::Sleep( 1000 );
		if( nLoop > 60 * 3 ) {
			QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::FreshCache() : overtime (>=3 min)" );
			return -3;
		}
	}

	FreeApi();														///< �ͷ�api����������
	CriticalLock	section( m_oLock );
	unsigned int	nSize = m_mapBasicData.size();
	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::FreshCache() : ............. [OK] basic data freshed(%d) ...........", nSize );

	return nSize;
}

int CTPQuoImage::FreeApi()
{
	m_nTrdReqID = 0;						///< ��������ID
	m_bIsResponded = false;

	QuotationSync::CTPSyncSaver::GetHandle().Release( true );

	if( m_pTraderApi )
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::FreeApi() : free api ......" );
		m_pTraderApi->Release();
		m_pTraderApi = NULL;
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::FreeApi() : done! ......" );
	}

	SimpleThread::Sleep( 1000*2 );

	return 0;
}

void CTPQuoImage::BreakOutWaitingResponse()
{
	CriticalLock	section( m_oLock );

	m_nTrdReqID = 0;						///< ��������ID
	m_bIsResponded = true;					///< ֹͣ�ȴ����ݷ���
	m_mapBasicData.clear();					///< ������л�������
}

void CTPQuoImage::SendLoginRequest()
{
	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::SendLoginRequest() : sending trd login message" );

	CThostFtdcReqUserLoginField	reqUserLogin = { 0 };

	memcpy( reqUserLogin.BrokerID, Configuration::GetConfig().GetTrdConfList().m_sParticipant.c_str(), Configuration::GetConfig().GetTrdConfList().m_sParticipant.length() );
	memcpy( reqUserLogin.UserID, Configuration::GetConfig().GetTrdConfList().m_sUID.c_str(), Configuration::GetConfig().GetTrdConfList().m_sUID.length() );
	memcpy( reqUserLogin.Password, Configuration::GetConfig().GetTrdConfList().m_sPswd.c_str(), Configuration::GetConfig().GetTrdConfList().m_sPswd.length() );
	strcpy( reqUserLogin.UserProductInfo,"�Ϻ�Ǭ¡�߿Ƽ����޹�˾" );
	strcpy( reqUserLogin.TradingDay, m_pTraderApi->GetTradingDay() );

	if( 0 == m_pTraderApi->ReqUserLogin( &reqUserLogin, 0 ) )
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::SendLoginRequest() : login message sended!" );
	}
	else
	{
		QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::SendLoginRequest() : failed 2 send login message" );
	}
}

void CTPQuoImage::OnHeartBeatWarning( int nTimeLapse )
{
	QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::OnHeartBeatWarning() : hb overtime, (%d)", nTimeLapse );
}

void CTPQuoImage::OnFrontConnected()
{
	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::OnFrontConnected() : connection established." );

    SendLoginRequest();
}

void CTPQuoImage::OnFrontDisconnected( int nReason )
{
	const char*		pszInfo=nReason == 0x1001 ? "�����ʧ��" :
							nReason == 0x1002 ? "����дʧ��" :
							nReason == 0x2001 ? "����������ʱ" :
							nReason == 0x2002 ? "��������ʧ��" :
							nReason == 0x2003 ? "�յ�������" :
							"δ֪ԭ��";

	BreakOutWaitingResponse();

	QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::OnFrontDisconnected() : connection disconnected, [reason:%d] [info:%s]", nReason, pszInfo );
}

void CTPQuoImage::OnRspUserLogin( CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
    if( pRspInfo->ErrorID != 0 )
	{
		// �˵�ʧ�ܣ��ͻ�������д�����
		QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::OnRspUserLogin() : failed 2 login [Err=%d,ErrMsg=%s,RequestID=%d,Chain=%d]"
											, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );

		SimpleThread::Sleep( 1000*6 );
        SendLoginRequest();
    }
	else
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::OnRspUserLogin() : succeed 2 login [RequestID=%d,Chain=%d]", nRequestID, bIsLast );

		///�����ѯ��Լ
		CThostFtdcQryInstrumentField	tagQueryParam = { 0 };
		::strncpy( tagQueryParam.ExchangeID, Configuration::GetConfig().GetExchangeID().c_str(), Configuration::GetConfig().GetExchangeID().length() );

		int	nRet = m_pTraderApi->ReqQryInstrument( &tagQueryParam, ++m_nTrdReqID );
		if( nRet >= 0 )
		{
			QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::OnRspUserLogin() : [OK] Instrument list requested! errorcode=%d", nRet );
		}
		else
		{
			BreakOutWaitingResponse();
			QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::OnRspUserLogin() : [ERROR] Failed 2 request instrument list, errorcode=%d", nRet );
		}
	}
}

void CTPQuoImage::OnRspQryInstrument( CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	if( NULL != pRspInfo )
	{
		if( pRspInfo->ErrorID != 0 )		///< ��ѯʧ��
		{
			BreakOutWaitingResponse();

			QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::OnRspQryInstrument() : failed 2 query instrument [Err=%d,ErrMsg=%s,RequestID=%d,Chain=%d]"
												, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );

			return;
		}
	}

	if( NULL != pInstrument )
	{
		QuotationSync::CTPSyncSaver::GetHandle().SaveStaticData( *pInstrument );

		if( false == m_bIsResponded )
		{	///< �ж�Ϊ�Ƿ���Ҫ���˵���Ʒ
			CriticalLock					section( m_oLock );
			tagCTPReferenceData				tagName = { 0 };							///< ��Ʒ������Ϣ�ṹ
			tagCTPRefParameter				tagParam = { 0 };
			tagCTPSnapData					tagSnapTable = { 0 };	///< ���սṹ
			CThostFtdcInstrumentField&		refSnap = *pInstrument;						///< ��������ӿڷ��ؽṹ

			m_mapBasicData[std::string(pInstrument->InstrumentID)] = *pInstrument;

			::strncpy( tagName.Code, refSnap.InstrumentID, sizeof(tagName.Code) );		///< ��Ʒ����
			::strncpy( tagParam.Code, refSnap.InstrumentID, sizeof(tagParam.Code) );		///< ��Ʒ����
			::memcpy( tagSnapTable.Code, refSnap.InstrumentID, sizeof(tagSnapTable.Code) );
			::strncpy( tagName.Name, refSnap.InstrumentName, sizeof(tagName.Code) );	///< ��Ʒ����
//			tagName.Type = QuotationData::GetMkInfo().GetCategoryIndication( refMkID );	///< ��Ȩ�ķ���
//			tagName.ObjectMId = refMkID.GetUnderlyingMarketID();						///< ����г����, �Ϻ��ڻ� 0  �����ڻ� 1  ֣���ڻ� 2 �Ϻ���Ȩ 3  ������Ȩ 4  ֣����Ȩ 5
//			tagName.LotFactor = refMkRules[refMkID.ParsePreNameFromCode(tagName.Code)].nLotFactor;///< ���׵�λ(�ֱ���)֧��С��
			tagName.DeliveryDate = ::atol(refSnap.StartDelivDate);						///< ������(YYYYMMDD)
			tagName.StartDate = ::atol(refSnap.OpenDate);								///< �׸�������(YYYYMMDD)
			tagName.EndDate = ::atol(refSnap.ExpireDate);								///< �������(YYYYMMDD), �� ������
			tagName.ExpireDate = tagName.EndDate;										///< ������(YYYYMMDD)
			tagName.ContractMult = refSnap.VolumeMultiple;
//			if( EID_Mk_OPTION == CTPMkID::SecurityType() )
			{
				///< ��Ĵ���
				::memcpy( tagName.UnderlyingCode, refSnap.UnderlyingInstrID, sizeof(tagName.UnderlyingCode) );
				tagName.PriceLimitType = 'N';											///< �ǵ�����������(N ���ǵ���)(R ���ǵ���)
				tagName.LotSize = 1;													///< �ֱ��ʣ���ȨΪ1��
//				tagName.XqDate = refMkID.ParseExerciseDateFromCode( tagName.Code );		///< ��Ȩ��(YYYYMM), ������code
				///< ��Ȩ�۸�(��ȷ����) //[*�Ŵ���] 
//				tagName.XqPrice = refSnap.StrikePrice*QuotationData::GetMkInfo().GetRateByCategory(tagName.Type)+0.5;
			}
/*			else if( EID_Mk_FUTURE == CTPMkID::SecurityType() )
			{
				tagName.LotSize = refMkRules[refMkID.ParsePreNameFromCode(tagName.Code)].nFutLotSize;	///< �ֱ���
			}*/

//			tagName.TypePeriodIdx = refMkRules[refMkID.ParsePreNameFromCode(tagName.Code)].nPeriodIdx;	///< ���ཻ��ʱ���λ��
/*			const DataRules::tagPeriods& oPeriod = refMkRules[tagName.TypePeriodIdx];
			int	stime = ((oPeriod.nPeriod[0][0]/60)*100 + oPeriod.nPeriod[0][0]%60)*100;				///< ��Լ�Ľ���ʱ�ε���ʼ��
			if( stime == EARLY_OPEN_TIME ) {
				tagName.EarlyNightFlag = 1;
			} else {
				tagName.EarlyNightFlag = 2;
			}*/

			QuoCollector::GetCollector()->OnImage( 1000, (char*)&tagName, sizeof(tagName), bIsLast );
			QuoCollector::GetCollector()->OnImage( 1001, (char*)&tagParam, sizeof(tagParam), bIsLast );
			QuoCollector::GetCollector()->OnImage( 1002, (char*)&tagSnapTable, sizeof(tagSnapTable), bIsLast );
		}
	}

	if( true == bIsLast )
	{	///< ���һ��
		CriticalLock	section( m_oLock );
		m_bIsResponded = true;				///< ֹͣ�ȴ����ݷ���(���󷵻����[OK])
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::OnRspQryInstrument() : [DONE] basic data freshed! size=%d", m_mapBasicData.size() );
	}
}

void CTPQuoImage::OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	QuoCollector::GetCollector()->OnLog( TLV_ERROR, "CTPQuoImage::OnRspError() : ErrorCode=[%d], ErrorMsg=[%s],RequestID=[%d], Chain=[%d]"
										, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );

	BreakOutWaitingResponse();
}








