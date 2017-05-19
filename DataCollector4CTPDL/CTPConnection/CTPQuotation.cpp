#include <algorithm>
#include "CTPQuotation.h"
#include "../DataCollector4CTPDL.h"
#pragma comment(lib, "./CTPConnection/thostmduserapi.lib")


CTPWorkStatus::CTPWorkStatus()
: m_eWorkStatus( ET_SS_UNACTIVE )
{
}

CTPWorkStatus::CTPWorkStatus( const CTPWorkStatus& refStatus )
{
	CriticalLock	section( m_oLock );

	m_eWorkStatus = refStatus.m_eWorkStatus;
}

CTPWorkStatus::operator enum E_SS_Status()
{
	CriticalLock	section( m_oLock );

	return m_eWorkStatus;
}

std::string& CTPWorkStatus::CastStatusStr( enum E_SS_Status eStatus )
{
	static std::string	sUnactive = "未激活";
	static std::string	sDisconnected = "断开状态";
	static std::string	sConnected = "连通状态";
	static std::string	sLogin = "登录成功";
	static std::string	sInitialized = "初始化中";
	static std::string	sWorking = "推送行情中";
	static std::string	sUnknow = "不可识别状态";

	switch( eStatus )
	{
	case ET_SS_UNACTIVE:
		return sUnactive;
	case ET_SS_DISCONNECTED:
		return sDisconnected;
	case ET_SS_CONNECTED:
		return sConnected;
	case ET_SS_LOGIN:
		return sLogin;
	case ET_SS_INITIALIZING:
		return sInitialized;
	case ET_SS_WORKING:
		return sWorking;
	default:
		return sUnknow;
	}
}

CTPWorkStatus&	CTPWorkStatus::operator= ( enum E_SS_Status eWorkStatus )
{
	CriticalLock	section( m_oLock );

	if( m_eWorkStatus != eWorkStatus )
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPWorkStatus::operator=() : Exchange CTP Session Status [%s]->[%s]"
											, CastStatusStr(m_eWorkStatus).c_str(), CastStatusStr(eWorkStatus).c_str() );
				
		m_eWorkStatus = eWorkStatus;
	}

	return *this;
}


///< ----------------------------------------------------------------


CTPQuotation::CTPQuotation()
 : m_pCTPApi( NULL )
{
}

CTPQuotation::~CTPQuotation()
{
	Destroy();
}

CTPWorkStatus& CTPQuotation::GetWorkStatus()
{
	return m_oWorkStatus;
}

int CTPQuotation::Activate()
{
	if( GetWorkStatus() == ET_SS_UNACTIVE )
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::Activate() : ............ CTP Session Activating............" );
		CTPLinkConfig&		refConfig = Configuration::GetConfig().GetHQConfList();

		Destroy();
		m_pCTPApi = CThostFtdcMdApi::CreateFtdcMdApi();					///< 从CTP的DLL导出新的api接口
		if( NULL == m_pCTPApi )
		{
			QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuotation::Activate() : error occur while creating CTP control api" );
			return -3;
		}

		m_pCTPApi->RegisterSpi( this );									///< 将this注册为事件处理的实例
		if( false == refConfig.RegisterServer( m_pCTPApi, NULL ) )		///< 注册CTP链接需要的网络配置
		{
			QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuotation::Activate() : invalid front/name server address" );
			return -4;
		}

		m_pCTPApi->Init();												///< 使客户端开始与行情发布服务器建立连接
		m_oWorkStatus = ET_SS_DISCONNECTED;				///< 更新CTPQuotation会话的状态
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::Activate() : ............ CTPQuotation Activated!.............." );
	}

	return 0;
}

int CTPQuotation::Destroy()
{
	if( m_pCTPApi )
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::Destroy() : ............ Destroying .............." );

		m_pCTPApi->RegisterSpi(NULL);
		m_pCTPApi->Release();
		m_pCTPApi = NULL;

		m_oWorkStatus = ET_SS_UNACTIVE;	///< 更新CTPQuotation会话的状态
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::Destroy() : ............ Destroyed! .............." );
	}

	return 0;
}

int CTPQuotation::SubscribeQuotation()
{
	if( GetWorkStatus() == ET_SS_LOGIN )			///< 登录成功后，执行订阅操作
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::SubscribeQuotation() : [ATTENTION] - Quotation Is Subscribing................" );

		char				pszCodeList[1024*5][20] = { 0 };	///< 订阅代码缓存
		char*				pszCodes[1024*5] = { NULL };		///< 待订阅代码列表,从各市场基础数据中提取
		int					nRet = QuoCollector::GetCollector().GetSubscribeCodeList( pszCodeList, 1024*5 );

		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::SubscribeQuotation() : Argument Num = %d.", nRet );
		for( int n = 0; n < nRet; n++ ) {
			pszCodes[n] = pszCodeList[n]+0;
		}

		if( nRet > 0 ) {
			QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::SubscribeQuotation() : Creating ctp session\'s dump file ......" );

			DateTime		oTodayDate;
			char			pszTmpFile[128] = { 0 };			///< 准备行情数据落盘
			unsigned int	nNowTime = DateTime::Now().TimeToLong();

			oTodayDate.SetCurDateTime();
			if( nNowTime > 40000 && nNowTime < 180000 ) {
				::strcpy( pszTmpFile, "Quotation_day.dmp" );
			} else if( nNowTime > 0 && nNowTime < 40000 ) {
				oTodayDate -= (60*60*8);
				::strcpy( pszTmpFile, "Quotation_nite.dmp" );
			} else {
				::strcpy( pszTmpFile, "Quotation_nite.dmp" );
			}
			std::string		sDumpFile = GenFilePathByWeek( Configuration::GetConfig().GetDumpFolder().c_str(), pszTmpFile, oTodayDate.Now().DateToLong() );
			bool			bRet = QuotationSync::CTPSyncSaver::GetHandle().Init( sDumpFile.c_str(), DateTime::Now().DateToLong(), false );

			QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::SubscribeQuotation() : dump file created, result = %d", bRet );
		}

		m_oWorkStatus = ET_SS_INITIALIZING;		///< 更新CTPQuotation会话的状态
		if( nRet > 0 ) { nRet = m_pCTPApi->SubscribeMarketData( pszCodes, nRet ); }
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::SubscribeQuotation() : [ATTENTION] - Quotation Has Been Subscribed! errorcode(%d) !!!", nRet );
		return 0;
	}

	return -2;
}

void CTPQuotation::SendLoginRequest()
{
	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::SendLoginRequest() : sending hq login message" );

	CThostFtdcReqUserLoginField	reqUserLogin = { 0 };

	strcpy( reqUserLogin.UserProductInfo,"上海乾隆高科技有限公司" );
	strcpy( reqUserLogin.TradingDay, m_pCTPApi->GetTradingDay() );
	memcpy( reqUserLogin.Password, Configuration::GetConfig().GetHQConfList().m_sPswd.c_str(), Configuration::GetConfig().GetHQConfList().m_sPswd.length() );
	memcpy( reqUserLogin.UserID, Configuration::GetConfig().GetHQConfList().m_sUID.c_str(), Configuration::GetConfig().GetHQConfList().m_sUID.length() );
	memcpy( reqUserLogin.BrokerID, Configuration::GetConfig().GetHQConfList().m_sParticipant.c_str(), Configuration::GetConfig().GetHQConfList().m_sParticipant.length() );

	if( 0 == m_pCTPApi->ReqUserLogin( &reqUserLogin, 0 ) )
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::SendLoginRequest() : login message sended!" );
	}
	else
	{
		QuoCollector::GetCollector()->OnLog( TLV_ERROR, "CTPQuotation::SendLoginRequest() : failed 2 send login message" );
	}
}

void CTPQuotation::OnFrontConnected()
{
	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::OnFrontConnected() : connection established." );

	m_oWorkStatus = ET_SS_CONNECTED;				///< 更新CTPQuotation会话的状态

    SendLoginRequest();
}

void CTPQuotation::OnFrontDisconnected( int nReason )
{
	const char*		pszInfo=nReason == 0x1001 ? "网络读失败" :
							nReason == 0x1002 ? "网络写失败" :
							nReason == 0x2001 ? "接收心跳超时" :
							nReason == 0x2002 ? "发送心跳失败" :
							nReason == 0x2003 ? "收到错误报文" :
							"未知原因";

	QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuotation::OnFrontDisconnected() : connection disconnected, [reason:%d] [info:%s]", nReason, pszInfo );

	m_oWorkStatus = ET_SS_DISCONNECTED;			///< 更新CTPQuotation会话的状态
}

void CTPQuotation::OnHeartBeatWarning( int nTimeLapse )
{
	QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuotation::OnHeartBeatWarning() : hb overtime, (%d)", nTimeLapse );
}

void CTPQuotation::OnRspUserLogin( CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	CriticalLock	section( m_oLock );

	m_setRecvCode.clear();				///< 清空收到的代码集合记录

    if( pRspInfo->ErrorID != 0 )
	{
		// 端登失败，客户端需进行错误处理
		QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuotation::OnRspUserLogin() : failed 2 login [Err=%d,ErrMsg=%s,RequestID=%d,Chain=%d]"
											, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );

		m_oWorkStatus = ET_SS_CONNECTED;			///< 更新CTPQuotation会话的状态
        Sleep( 3000 );
        SendLoginRequest();
    }
	else
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::OnRspUserLogin() : succeed 2 login [RequestID=%d,Chain=%d]", nRequestID, bIsLast );
		m_oWorkStatus = ET_SS_LOGIN;				///< 更新CTPQuotation会话的状态
		SubscribeQuotation();
	}
}

void CTPQuotation::OnRtnDepthMarketData( CThostFtdcDepthMarketDataField *pMarketData )
{
	if( NULL == pMarketData )
	{
		QuoCollector::GetCollector()->OnLog( TLV_ERROR, "CTPQuotation::OnRtnDepthMarketData() : invalid market data pointer (NULL)" );
		return;
	}

	QuotationSync::CTPSyncSaver::GetHandle().SaveSnapData( *pMarketData );

	///< 判断是否收完全幅快照(以收到的代码是否有重复为判断依据)
	bool	bInitializing = (enum E_SS_Status)m_oWorkStatus != ET_SS_WORKING;
	if( true == bInitializing )
	{
		CriticalLock	section( m_oLock );

		if( m_setRecvCode.find( pMarketData->InstrumentID ) == m_setRecvCode.end() )
		{
			m_setRecvCode.insert( pMarketData->InstrumentID );
		}
		else
		{
			m_oWorkStatus = ET_SS_WORKING;	///< 收到重复代码，全幅快照已收完整
			bInitializing = true;
		}
	}

	FlushQuotation( pMarketData, bInitializing );
}

void CTPQuotation::FlushQuotation( CThostFtdcDepthMarketDataField* pQuotationData, bool bInitialize )
{
	double							dRate = 1.;				///< 放大倍数
	int								nSerial = 0;			///< 商品在码表的索引值
	tagCTPSnapData					tagSnapTable = { 0 };	///< 快照结构
	unsigned int					nSnapTradingDate = 0;	///< 快照交易日期

	::memcpy( tagSnapTable.Code, pQuotationData->InstrumentID, sizeof(tagSnapTable.Code) );
/*	if( (nSerial = CodeHash::GetInstance().Code2Serial( tagSnapTable.Code, eMkID )) < 0 )
	{
		return;
	}*/

	tagCTPReferenceData	refNameTable;
//	tagCTPSnapData&					refNameTable = m_oRefTable[nSerial];	///< 码表结构
//	dRate = QuotationData::GetMkInfo().GetRateByCategory( refNameTable.Type );
//	tagSnapTable = m_oSnapTable[nSerial];
	//assert( strncmp( refNameTable.Code, tagSnapTable.Code, sizeof(tagSnapTable.Code) )==0 );

/*	if( true == bInitialize ) {	///< 初始化行情
		refNameTable.LeavesQty = pQuotationData->PreOpenInterest*dRate+0.5;
	}
	if( pQuotationData->UpperLimitPrice > 0 ) {
		refNameTable.UpLimit = pQuotationData->UpperLimitPrice*dRate+0.5;
	}
	if( pQuotationData->LowerLimitPrice > 0 ) {
		refNameTable.DownLimit = pQuotationData->LowerLimitPrice*dRate+0.5;
	}*/

	tagSnapTable.UpperPrice = pQuotationData->UpperLimitPrice*dRate+0.5;
	tagSnapTable.LowerPrice = pQuotationData->LowerLimitPrice*dRate+0.5;
	tagSnapTable.Open = pQuotationData->OpenPrice*dRate+0.5;
	tagSnapTable.High = pQuotationData->HighestPrice*dRate+0.5;
	tagSnapTable.Low = pQuotationData->LowestPrice*dRate+0.5;
	tagSnapTable.Close = pQuotationData->ClosePrice*dRate+0.5;
	tagSnapTable.Now = pQuotationData->LastPrice*dRate+0.5;
	tagSnapTable.PreClose = pQuotationData->PreClosePrice*dRate+0.5;
	tagSnapTable.PreSettlePrice = pQuotationData->PreSettlementPrice*dRate+0.5;
	tagSnapTable.SettlePrice = pQuotationData->SettlementPrice*dRate+0.5;
//	if( EV_MK_ZZ == eMkID )		///< 郑州市场的成交金额特殊处理： = 金额 * 合约单位
	{
//		tagSnapTable.Amount = pQuotationData->Turnover * refNameTable.ContractMult;
	}
//	else
	{
		tagSnapTable.Amount = pQuotationData->Turnover;
	}
	tagSnapTable.Volume = pQuotationData->Volume;
	tagSnapTable.PreOpenInterest = pQuotationData->PreOpenInterest;
	tagSnapTable.OpenInterest = pQuotationData->OpenInterest;
	tagSnapTable.Buy[0].Price = pQuotationData->BidPrice1*dRate+0.5;
	tagSnapTable.Buy[0].Volume = pQuotationData->BidVolume1;
	tagSnapTable.Sell[0].Price = pQuotationData->AskPrice1*dRate+0.5;
	tagSnapTable.Sell[0].Volume = pQuotationData->AskVolume1;
	tagSnapTable.Buy[1].Price = pQuotationData->BidPrice2*dRate+0.5;
	tagSnapTable.Buy[1].Volume = pQuotationData->BidVolume2;
	tagSnapTable.Sell[1].Price = pQuotationData->AskPrice2*dRate+0.5;
	tagSnapTable.Sell[1].Volume = pQuotationData->AskVolume2;
	tagSnapTable.Buy[2].Price = pQuotationData->BidPrice3*dRate+0.5;
	tagSnapTable.Buy[2].Volume = pQuotationData->BidVolume3;
	tagSnapTable.Sell[2].Price = pQuotationData->AskPrice3*dRate+0.5;
	tagSnapTable.Sell[2].Volume = pQuotationData->AskVolume3;
	tagSnapTable.Buy[3].Price = pQuotationData->BidPrice4*dRate+0.5;
	tagSnapTable.Buy[3].Volume = pQuotationData->BidVolume4;
	tagSnapTable.Sell[3].Price = pQuotationData->AskPrice4*dRate+0.5;
	tagSnapTable.Sell[3].Volume = pQuotationData->AskVolume4;
	tagSnapTable.Buy[4].Price = pQuotationData->BidPrice5*dRate+0.5;
	tagSnapTable.Buy[4].Volume = pQuotationData->BidVolume5;
	tagSnapTable.Sell[4].Price = pQuotationData->AskPrice5*dRate+0.5;
	tagSnapTable.Sell[4].Volume = pQuotationData->AskVolume5;

	char	pszTmpDate[12] = { 0 };
	::memcpy( pszTmpDate, pQuotationData->UpdateTime, sizeof(TThostFtdcTimeType) );
	pszTmpDate[2] = 0;
	pszTmpDate[5] = 0;
	pszTmpDate[8] = 0;
	int		nSnapUpdateTime = atol(pszTmpDate);
	nSnapUpdateTime = nSnapUpdateTime*100+atol(&pszTmpDate[3]);
	nSnapUpdateTime = nSnapUpdateTime*100+atol(&pszTmpDate[6]);
	if( (nSnapTradingDate=::atol( pQuotationData->TradingDay )) >= 0 && nSnapUpdateTime > 0 )
	{	///< 更新日期+时间
		tagSnapTable.DataTimeStamp = nSnapUpdateTime*1000;
//		GetMkInfo().SetDateTime( nSnapTradingDate, nSnapUpdateTime );
	}

	if( true == bInitialize )
	{
//		QuoCollector::GetCollector()->OnImage( id, data, len, lastflag );
	}
	else
	{
//		QuoCollector::GetCollector()->OnData( ID, pdatga, len, pushflag );
	}
}

void CTPQuotation::OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	QuoCollector::GetCollector()->OnLog( TLV_ERROR, "CTPQuotation::OnRspError() : [%s] ErrorCode=[%d], ErrorMsg=[%s],RequestID=[%d], Chain=[%d]"
										, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );
}






