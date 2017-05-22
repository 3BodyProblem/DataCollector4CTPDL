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

	FreeApi();///< 清理上下文 && 创建api控制对象
	if( NULL == (m_pTraderApi=CThostFtdcTraderApi::CreateFtdcTraderApi()) )
	{
		QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::FreshCache() : error occur while creating CTP trade control api" );
		return -1;
	}

	char			pszTmpFile[128] = { 0 };						///< 准备请求的静态数据落盘
	unsigned int	nNowTime = DateTime::Now().TimeToLong();
	if( nNowTime > 80000 && nNowTime < 110000 )
		::strcpy( pszTmpFile, "Trade_am.dmp" );
	else
		::strcpy( pszTmpFile, "Trade_pm.dmp" );
	std::string		sDumpFile = GenFilePathByWeek( Configuration::GetConfig().GetDumpFolder().c_str(), pszTmpFile, DateTime::Now().DateToLong() );
	QuotationSync::CTPSyncSaver::GetHandle().Init( sDumpFile.c_str(), DateTime::Now().DateToLong(), true );

	m_pTraderApi->RegisterSpi( this );								///< 将this注册为事件处理的实例
	if( false == Configuration::GetConfig().GetTrdConfList().RegisterServer( NULL, m_pTraderApi ) )///< 注册CTP链接需要的网络配置
	{
		QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::FreshCache() : invalid front/name server address" );
		return -2;
	}

	m_pTraderApi->Init();											///< 使客户端开始与行情发布服务器建立连接
	for( int nLoop = 0; false == m_bIsResponded; nLoop++ )			///< 等待请求响应结束
	{
		SimpleThread::Sleep( 1000 );
		if( nLoop > 60 * 3 ) {
			QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::FreshCache() : overtime (>=3 min)" );
			return -3;
		}
	}

	FreeApi();														///< 释放api，结束请求
	CriticalLock	section( m_oLock );
	unsigned int	nSize = m_mapBasicData.size();
	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::FreshCache() : ............. [OK] basic data freshed(%d) ...........", nSize );

	return nSize;
}

int CTPQuoImage::FreeApi()
{
	m_nTrdReqID = 0;						///< 重置请求ID
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

	m_nTrdReqID = 0;						///< 重置请求ID
	m_bIsResponded = true;					///< 停止等待数据返回
	m_mapBasicData.clear();					///< 清空所有缓存数据
}

void CTPQuoImage::SendLoginRequest()
{
	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::SendLoginRequest() : sending trd login message" );

	CThostFtdcReqUserLoginField	reqUserLogin = { 0 };

	memcpy( reqUserLogin.BrokerID, Configuration::GetConfig().GetTrdConfList().m_sParticipant.c_str(), Configuration::GetConfig().GetTrdConfList().m_sParticipant.length() );
	memcpy( reqUserLogin.UserID, Configuration::GetConfig().GetTrdConfList().m_sUID.c_str(), Configuration::GetConfig().GetTrdConfList().m_sUID.length() );
	memcpy( reqUserLogin.Password, Configuration::GetConfig().GetTrdConfList().m_sPswd.c_str(), Configuration::GetConfig().GetTrdConfList().m_sPswd.length() );
	strcpy( reqUserLogin.UserProductInfo,"上海乾隆高科技有限公司" );
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
	const char*		pszInfo=nReason == 0x1001 ? "网络读失败" :
							nReason == 0x1002 ? "网络写失败" :
							nReason == 0x2001 ? "接收心跳超时" :
							nReason == 0x2002 ? "发送心跳失败" :
							nReason == 0x2003 ? "收到错误报文" :
							"未知原因";

	BreakOutWaitingResponse();

	QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::OnFrontDisconnected() : connection disconnected, [reason:%d] [info:%s]", nReason, pszInfo );
}

void CTPQuoImage::OnRspUserLogin( CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
    if( pRspInfo->ErrorID != 0 )
	{
		// 端登失败，客户端需进行错误处理
		QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::OnRspUserLogin() : failed 2 login [Err=%d,ErrMsg=%s,RequestID=%d,Chain=%d]"
											, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );

		SimpleThread::Sleep( 1000*6 );
        SendLoginRequest();
    }
	else
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::OnRspUserLogin() : succeed 2 login [RequestID=%d,Chain=%d]", nRequestID, bIsLast );

		///请求查询合约
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
		if( pRspInfo->ErrorID != 0 )		///< 查询失败
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
		{	///< 判断为是否需要过滤的商品
			CriticalLock					section( m_oLock );
			tagCTPReferenceData				tagName = { 0 };							///< 商品基础信息结构
			tagCTPRefParameter				tagParam = { 0 };
			tagCTPSnapData					tagSnapTable = { 0 };	///< 快照结构
			CThostFtdcInstrumentField&		refSnap = *pInstrument;						///< 交易请求接口返回结构

			m_mapBasicData[std::string(pInstrument->InstrumentID)] = *pInstrument;

			::strncpy( tagName.Code, refSnap.InstrumentID, sizeof(tagName.Code) );		///< 商品代码
			::strncpy( tagParam.Code, refSnap.InstrumentID, sizeof(tagParam.Code) );		///< 商品代码
			::memcpy( tagSnapTable.Code, refSnap.InstrumentID, sizeof(tagSnapTable.Code) );
			::strncpy( tagName.Name, refSnap.InstrumentName, sizeof(tagName.Code) );	///< 商品名称
//			tagName.Type = QuotationData::GetMkInfo().GetCategoryIndication( refMkID );	///< 期权的分类
//			tagName.ObjectMId = refMkID.GetUnderlyingMarketID();						///< 标的市场编号, 上海期货 0  大连期货 1  郑州期货 2 上海期权 3  大连期权 4  郑州期权 5
//			tagName.LotFactor = refMkRules[refMkID.ParsePreNameFromCode(tagName.Code)].nLotFactor;///< 交易单位(手比率)支持小数
			tagName.DeliveryDate = ::atol(refSnap.StartDelivDate);						///< 交割日(YYYYMMDD)
			tagName.StartDate = ::atol(refSnap.OpenDate);								///< 首个交易日(YYYYMMDD)
			tagName.EndDate = ::atol(refSnap.ExpireDate);								///< 最后交易日(YYYYMMDD), 即 到期日
			tagName.ExpireDate = tagName.EndDate;										///< 到期日(YYYYMMDD)
			tagName.ContractMult = refSnap.VolumeMultiple;
//			if( EID_Mk_OPTION == CTPMkID::SecurityType() )
			{
				///< 标的代码
				::memcpy( tagName.UnderlyingCode, refSnap.UnderlyingInstrID, sizeof(tagName.UnderlyingCode) );
				tagName.PriceLimitType = 'N';											///< 涨跌幅限制类型(N 有涨跌幅)(R 无涨跌幅)
				tagName.LotSize = 1;													///< 手比率，期权为1张
//				tagName.XqDate = refMkID.ParseExerciseDateFromCode( tagName.Code );		///< 行权日(YYYYMM), 解析自code
				///< 行权价格(精确到厘) //[*放大倍数] 
//				tagName.XqPrice = refSnap.StrikePrice*QuotationData::GetMkInfo().GetRateByCategory(tagName.Type)+0.5;
			}
/*			else if( EID_Mk_FUTURE == CTPMkID::SecurityType() )
			{
				tagName.LotSize = refMkRules[refMkID.ParsePreNameFromCode(tagName.Code)].nFutLotSize;	///< 手比率
			}*/

//			tagName.TypePeriodIdx = refMkRules[refMkID.ParsePreNameFromCode(tagName.Code)].nPeriodIdx;	///< 分类交易时间段位置
/*			const DataRules::tagPeriods& oPeriod = refMkRules[tagName.TypePeriodIdx];
			int	stime = ((oPeriod.nPeriod[0][0]/60)*100 + oPeriod.nPeriod[0][0]%60)*100;				///< 合约的交易时段的起始点
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
	{	///< 最后一条
		CriticalLock	section( m_oLock );
		m_bIsResponded = true;				///< 停止等待数据返回(请求返回完成[OK])
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::OnRspQryInstrument() : [DONE] basic data freshed! size=%d", m_mapBasicData.size() );
	}
}

void CTPQuoImage::OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	QuoCollector::GetCollector()->OnLog( TLV_ERROR, "CTPQuoImage::OnRspError() : ErrorCode=[%d], ErrorMsg=[%s],RequestID=[%d], Chain=[%d]"
										, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );

	BreakOutWaitingResponse();
}








