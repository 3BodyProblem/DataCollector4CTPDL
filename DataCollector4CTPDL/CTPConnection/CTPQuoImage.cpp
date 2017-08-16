#include <math.h>
#include <string>
#include <algorithm>
#include "CTPQuoImage.h"
#include "../DataCollector4CTPDL.h"
#pragma comment(lib, "./CTPConnection/thosttraderapi.lib")


T_MAP_RATE		CTPQuoImage::m_mapRate;


CTPQuoImage::CTPQuoImage()
 : m_pTraderApi( NULL ), m_nTrdReqID( 0 ), m_bIsResponded( false )
{
}

CTPQuoImage::operator T_MAP_BASEDATA&()
{
	CriticalLock	section( m_oLock );

	return m_mapBasicData;
}

int CTPQuoImage::GetRate( unsigned int nKind )
{
	if( m_mapRate.find( nKind ) != m_mapRate.end() )
	{
		return ::pow( (double)10, (int)m_mapRate[nKind] );
	}

	return -1;
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

int CTPQuoImage::LoadDataFile( std::string sFilePath, bool bEchoOnly )
{
	if( false == QuotationSync::CTPSyncLoader::GetHandle().Init( sFilePath.c_str(), true ) )
	{
		QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::LoadDataFile() : failed 2 open static file : %s", sFilePath.c_str() );
		return -1;
	}

	while( true )
	{
		CThostFtdcInstrumentField	oData = { 0 };

		if( QuotationSync::CTPSyncLoader::GetHandle().GetStaticData( oData ) <= 0 )
		{
			break;
		}

		if( true == bEchoOnly )
		{
			::printf( "%s,%s\n", oData.InstrumentID, oData.InstrumentName );
		}
		else
		{
			OnRspQryInstrument( &oData, NULL, 0, false );
		}
	}

	return 0;
}

int CTPQuoImage::FreshCache()
{
	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::FreshCache() : ............ [%s] Freshing basic data ..............."
										, (false==Configuration::GetConfig().IsBroadcastModel())? "NORMAL" : "BROADCAST" );

	char			pszTmpFile[128] = { 0 };							///< 准备请求的静态数据落盘
	unsigned int	nNowTime = DateTime::Now().TimeToLong();
	if( nNowTime > 80000 && nNowTime < 110000 )
		::strcpy( pszTmpFile, "Trade_am.dmp" );
	else
		::strcpy( pszTmpFile, "Trade_pm.dmp" );
	std::string		sDumpFile = GenFilePathByWeek( Configuration::GetConfig().GetDumpFolder().c_str(), pszTmpFile, DateTime::Now().DateToLong() );

	m_mapRate.clear();													///< 清空放大倍数映射表

	if( false == Configuration::GetConfig().IsBroadcastModel() )
	{
		FreeApi();///< 清理上下文 && 创建api控制对象
		if( NULL == (m_pTraderApi=CThostFtdcTraderApi::CreateFtdcTraderApi()) )
		{
			QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::FreshCache() : error occur while creating CTP trade control api" );
			return -1;
		}

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
	}
	else
	{
		if( LoadDataFile( sDumpFile, false ) < 0 )
		{
			QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuoImage::FreshCache() : failed 2 load image data" );
			return -4;
		}
	}

	BuildBasicData();
	CriticalLock	section( m_oLock );
	unsigned int	nSize = m_mapBasicData.size();
	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuoImage::FreshCache() : ............. [OK] basic data freshed(%d) ...........", nSize );

	return nSize;
}

void CTPQuoImage::BuildBasicData()
{
	tagDLFutureMarketInfo_LF100		tagMkInfo = { 0 };
	tagDLFutureMarketStatus_HF102	tagStatus = { 0 };

	::strcpy( tagMkInfo.Key, "mkinfo" );
	tagMkInfo.WareCount = m_mapBasicData.size();
	tagMkInfo.MarketID = Configuration::GetConfig().GetMarketID();
	tagMkInfo.MarketDate = DateTime::Now().DateToLong();

	tagMkInfo.PeriodsCount = 4;					///< 交易时段信息设置
	tagMkInfo.MarketPeriods[0][0] = 21*60;		///< 第一段，取夜盘的时段的最大范围
	tagMkInfo.MarketPeriods[0][1] = 23*60+30;
	tagMkInfo.MarketPeriods[1][0] = 9*60;		///< 第二段
	tagMkInfo.MarketPeriods[1][1] = 10*60+15;
	tagMkInfo.MarketPeriods[2][0] = 10*60+30;	///< 第三段
	tagMkInfo.MarketPeriods[2][1] = 11*60+30;
	tagMkInfo.MarketPeriods[3][0] = 13*60+30;	///< 第四段
	tagMkInfo.MarketPeriods[3][1] = 15*60;

	///< 配置分类信息
	tagMkInfo.KindCount = 4;
	{
		tagDLFutureKindDetail_LF101		tagKind = { 0 };

		::strcpy( tagKind.Key, "0" );
		::strncpy( tagKind.KindName, "指数保留", 8 );
		tagKind.PriceRate = 0;
		tagKind.LotFactor = 0;
		m_mapRate[m_mapRate.size()] = tagKind.PriceRate;
		QuoCollector::GetCollector()->OnImage( 101, (char*)&tagKind, sizeof(tagKind), true );
	}
	{
		tagDLFutureKindDetail_LF101		tagKind = { 0 };

		::strcpy( tagKind.Key, "1" );
		::strncpy( tagKind.KindName, "大连期指", 8 );
		tagKind.PriceRate = 2;
		tagKind.LotFactor = 100;
		m_mapRate[m_mapRate.size()] = tagKind.PriceRate;
		QuoCollector::GetCollector()->OnImage( 101, (char*)&tagKind, sizeof(tagKind), true );
	}
	{
		tagDLFutureKindDetail_LF101		tagKind = { 0 };

		::strcpy( tagKind.Key, "2" );
		::strncpy( tagKind.KindName, "大连合约", 8 );
		tagKind.PriceRate = 2;
		tagKind.LotFactor = 100;
		m_mapRate[m_mapRate.size()] = tagKind.PriceRate;
		QuoCollector::GetCollector()->OnImage( 101, (char*)&tagKind, sizeof(tagKind), true );
	}

	::strcpy( tagStatus.Key, "mkstatus" );
	tagStatus.MarketStatus = 0;
	tagStatus.MarketTime = DateTime::Now().TimeToLong();

	QuoCollector::GetCollector()->OnImage( 100, (char*)&tagMkInfo, sizeof(tagMkInfo), true );
	QuoCollector::GetCollector()->OnImage( 102, (char*)&tagStatus, sizeof(tagStatus), true );
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
		if( false == Configuration::GetConfig().IsBroadcastModel() )
		{
			QuotationSync::CTPSyncSaver::GetHandle().SaveStaticData( *pInstrument );
		}

		if( false == m_bIsResponded && pInstrument->ProductClass == THOST_FTDC_PC_Futures )
		{	///< 判断为是否需要过滤的商品
			CThostFtdcInstrumentField&		refSnap = *pInstrument;						///< 交易请求接口返回结构
			CriticalLock					section( m_oLock );
			tagDLFutureReferenceData_LF103	tagName = { 0 };							///< 商品基础信息结构
			tagDLFutureSnapData_HF105		tagSnapHF = { 0 };							///< 高速行情快照
			tagDLFutureSnapData_LF104		tagSnapLF = { 0 };							///< 低速行情快照
			tagDLFutureSnapBuySell_HF106	tagSnapBS = { 0 };							///< 档位信息

			::strncpy( tagName.Code, refSnap.InstrumentID, sizeof(tagName.Code) );		///< 商品代码
			::memcpy( tagSnapHF.Code, refSnap.InstrumentID, sizeof(tagSnapHF.Code) );	///< 商品代码
			::memcpy( tagSnapLF.Code, refSnap.InstrumentID, sizeof(tagSnapLF.Code) );	///< 商品代码
			::memcpy( tagSnapBS.Code, refSnap.InstrumentID, sizeof(tagSnapBS.Code) );	///< 商品代码

			tagName.Kind = 2;
			tagName.DerivativeType = 0;
			tagName.DeliveryDate = ::atol(refSnap.StartDelivDate);						///< 交割日(YYYYMMDD)
			tagName.StartDate = ::atol(refSnap.OpenDate);								///< 首个交易日(YYYYMMDD)
			tagName.EndDate = ::atol(refSnap.ExpireDate);								///< 最后交易日(YYYYMMDD), 即 到期日
			tagName.ExpireDate = tagName.EndDate;										///< 到期日(YYYYMMDD)
			tagName.ContractMult = refSnap.VolumeMultiple;								///< 合约chen数
			::strncpy( tagName.Name, refSnap.InstrumentName, sizeof(tagName.Code) );	///< 商品名称
			if( 3 == tagName.Kind )
			{
				tagName.LotSize = 1;													///< 手比率，期权为1张
				::memcpy( tagName.UnderlyingCode, refSnap.UnderlyingInstrID, sizeof(tagName.UnderlyingCode) );///< 标的代码
				tagName.XqPrice = refSnap.StrikePrice*GetRate(tagName.Kind)+0.5;		///< 行权价格(精确到厘) //[*放大倍数] 
			}
			else if( 2 == tagName.Kind )
			{
				tagName.LotSize = 1;													///< 手比率
			}

			tagName.PriceTick = refSnap.PriceTick*m_mapRate[tagName.Kind]+0.5;							///< 行权价格(精确到厘) //[*放大倍数] 

			m_mapBasicData[std::string(pInstrument->InstrumentID)] = *pInstrument;
			QuoCollector::GetCollector()->OnImage( 103, (char*)&tagName, sizeof(tagName), bIsLast );
			QuoCollector::GetCollector()->OnImage( 104, (char*)&tagSnapLF, sizeof(tagSnapLF), bIsLast );
			QuoCollector::GetCollector()->OnImage( 105, (char*)&tagSnapHF, sizeof(tagSnapHF), bIsLast );
			QuoCollector::GetCollector()->OnImage( 106, (char*)&tagSnapBS, sizeof(tagSnapBS), bIsLast );
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








