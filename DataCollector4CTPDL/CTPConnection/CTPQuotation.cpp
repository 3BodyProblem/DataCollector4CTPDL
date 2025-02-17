#include <algorithm>
#include "CTPQuotation.h"
#include "../DataCollector4CTPDL.h"
#pragma comment(lib, "./CTPConnection/thostmduserapi.lib")

/*
PackagesLoopBuffer::PackagesLoopBuffer()
 : m_pPkgBuffer( NULL ), m_nMaxPkgBufSize( 0 )
 , m_nFirstPkgHeadPos( 0 ), m_nCurrentWritePos( 0 )
 , m_nCurrentPkgHeadPos( 0 )
{
}

PackagesLoopBuffer::~PackagesLoopBuffer()
{
	Release();
}

int PackagesLoopBuffer::Initialize( unsigned long nMaxBufSize )
{
	Release();

	CriticalLock	guard( m_oLock );

	if( NULL == (m_pPkgBuffer = new char[nMaxBufSize]) )
	{
		DataNodeService::GetSerivceObj().WriteError( "PackagesLoopBuffer::Instance() : failed 2 initialize package buffer, size = %d", nMaxBufSize );
		return -1;
	}

	m_nMaxPkgBufSize = nMaxBufSize;

	return 0;
}

void PackagesLoopBuffer::Release()
{
	if( NULL != m_pPkgBuffer )
	{
		CriticalLock	guard( m_oLock );

		delete [] m_pPkgBuffer;
		m_pPkgBuffer = NULL;
		m_nMaxPkgBufSize = 0;
		m_nFirstPkgHeadPos = 0;
		m_nCurrentWritePos = 0;
		m_nCurrentPkgHeadPos = 0;
	}
}

float PackagesLoopBuffer::GetPercentOfFreeSize()
{
	CriticalLock		guard( m_oLock );

	float	nFreeSize = ((m_nFirstPkgHeadPos + m_nMaxPkgBufSize - m_nCurrentWritePos - 1) % m_nMaxPkgBufSize) * 1.0;

	return nFreeSize / m_nMaxPkgBufSize * 100;
}

int PackagesLoopBuffer::PushBlock( unsigned int nDataID, const char* pData, unsigned int nDataSize, unsigned __int64 nSeqNo, unsigned int& nMsgCount, unsigned int& nBodySize )
{
	CriticalLock		guard( m_oLock );

	nMsgCount = 0;
	nBodySize = 0;
	if( NULL == m_pPkgBuffer || nDataSize == 0 || NULL == pData || m_nMaxPkgBufSize == 0 )	{
		assert( 0 );
		return -1;
	}

	///< 计算余下的空间
	int	nFreeSize = (m_nFirstPkgHeadPos + m_nMaxPkgBufSize - m_nCurrentWritePos - 1) % m_nMaxPkgBufSize;

	///< 判断当前空闲空间是否足够
	if( m_nCurrentPkgHeadPos == m_nCurrentWritePos )
	{
		///< (需启用新pkg的情况):	需要考虑message体以外因素的空间占用
		if( (sizeof(tagPackageHead)+sizeof(unsigned int)+nDataSize) > nFreeSize )
		{
			return -2;	///< 空间不足
		}
	}
	else///< (在当前pkg追加的情况):
	{
		if( nDataSize > nFreeSize )
		{
			return -3;	///< 空间不足
		}

		///< 如果数据包的id不等于前面的数据包id，则 (新启用一个pkg包)
		if( nDataID != *((unsigned int*)(m_pPkgBuffer+m_nCurrentPkgHeadPos)) )
		{
			m_nCurrentPkgHeadPos = m_nCurrentWritePos;
		}
	}

	///< 处理包头的信息 m_nCurrentPkgHeadPos
	if( m_nCurrentPkgHeadPos == m_nCurrentWritePos )
	{
		*((unsigned int*)(m_pPkgBuffer+m_nCurrentPkgHeadPos)) = nDataID;
		((tagPackageHead*)(m_pPkgBuffer+m_nCurrentPkgHeadPos+sizeof(unsigned int)))->nSeqNo = nSeqNo;
		((tagPackageHead*)(m_pPkgBuffer+m_nCurrentPkgHeadPos+sizeof(unsigned int)))->nMarketID = DataCollector::GetMarketID();
		((tagPackageHead*)(m_pPkgBuffer+m_nCurrentPkgHeadPos+sizeof(unsigned int)))->nMsgLength = nDataSize;
		m_nCurrentWritePos += (sizeof(tagPackageHead) + sizeof(unsigned int));
	}

	nBodySize = ((tagPackageHead*)(m_pPkgBuffer+m_nCurrentPkgHeadPos+sizeof(unsigned int)))->nMsgLength * nMsgCount;

	int				nConsecutiveFreeSize = m_nMaxPkgBufSize - m_nCurrentWritePos;
	if( nConsecutiveFreeSize >= nDataSize )
	{
		::memcpy( &m_pPkgBuffer[m_nCurrentWritePos], (char*)pData, nDataSize );
		m_nCurrentWritePos = (m_nCurrentWritePos + nDataSize) % m_nMaxPkgBufSize;
	}
	else
	{
		::memcpy( &m_pPkgBuffer[m_nCurrentWritePos], pData, nConsecutiveFreeSize );
		::memcpy( &m_pPkgBuffer[0], pData + nConsecutiveFreeSize, (nDataSize - nConsecutiveFreeSize) );
		m_nCurrentWritePos = (m_nCurrentWritePos + nDataSize) % m_nMaxPkgBufSize;
	}

	return 0;
}

int PackagesLoopBuffer::GetOnePkg( char* pBuff, unsigned int nBuffSize, unsigned int& nMsgID )
{
	CriticalLock		guard( m_oLock );
	tagPackageHead*		pPkgHead = (tagPackageHead*)(m_pPkgBuffer+m_nFirstPkgHeadPos+sizeof(unsigned int));
	if( NULL == pBuff || nBuffSize == 0 || NULL == m_pPkgBuffer || m_nMaxPkgBufSize == 0 )	{
		assert( 0 );
		return -1;
	}

	unsigned int		nTotalPkgSize = (pPkgHead->nMsgLength * pPkgHead->nMsgCount) + sizeof(tagPackageHead) + sizeof(unsigned int);	///< 带msgid的长度值
	int					nDataLen = (m_nCurrentWritePos + m_nMaxPkgBufSize - m_nFirstPkgHeadPos) % m_nMaxPkgBufSize;
	if( nDataLen == 0 )
	{
		return -2;
	}

	nMsgID = *((unsigned int*)(m_pPkgBuffer+m_nFirstPkgHeadPos));
	if( nBuffSize < nTotalPkgSize )
	{
		return -3;
	}
	else
	{
		nBuffSize = (nTotalPkgSize - sizeof(unsigned int));
	}

	int nConsecutiveSize = m_nMaxPkgBufSize - m_nFirstPkgHeadPos;
	if( nConsecutiveSize >= nBuffSize )
	{
		::memcpy( pBuff, &m_pPkgBuffer[m_nFirstPkgHeadPos + sizeof(unsigned int)], nBuffSize );
	}
	else
	{
		::memcpy( pBuff, &m_pPkgBuffer[m_nFirstPkgHeadPos + sizeof(unsigned int)], nConsecutiveSize );
		::memcpy( pBuff + nConsecutiveSize, m_pPkgBuffer+0, (nBuffSize - nConsecutiveSize) );
	}

	unsigned int		nLastFirstPkgHeadPos = m_nFirstPkgHeadPos;

	m_nFirstPkgHeadPos = (m_nFirstPkgHeadPos + nTotalPkgSize) % m_nMaxPkgBufSize;
	if( m_nCurrentPkgHeadPos == nLastFirstPkgHeadPos )
	{
		m_nCurrentPkgHeadPos = m_nFirstPkgHeadPos;
	}

	return nBuffSize;
}

bool PackagesLoopBuffer::IsEmpty()
{
	if( m_nFirstPkgHeadPos == m_nCurrentWritePos )
	{
		return true;
	}
	else
	{
		return false;
	}
}
*/

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
 : m_pCTPApi( NULL ), m_nCodeCount( 0 )
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
	if( true == Configuration::GetConfig().IsBroadcastModel() )
	{
		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::Activate() : ............ Broadcast Thread Activating............" );
		m_oWorkStatus = ET_SS_DISCONNECTED;								///< 更新CTPQuotation会话的状态

		if( 0 != SimpleTask::Activate() )
		{
			QuoCollector::GetCollector()->OnLog( TLV_WARN, "CTPQuotation::Activate() : failed 2 activate broadcast thread..." );
			return -1;
		}

		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::Activate() : ............ Broadcast Thread Activated!............" );
		return 0;
	}

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
		m_oWorkStatus = ET_SS_DISCONNECTED;								///< 更新CTPQuotation会话的状态
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
		m_oDataRecorder.CloseFile();		///< 关闭落盘文件的句柄
		m_oWorkStatus = ET_SS_UNACTIVE;		///< 更新CTPQuotation会话的状态

		QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::Destroy() : ............ Destroyed! .............." );
	}

	return 0;
}

int CTPQuotation::LoadDataFile( std::string sFilePath, bool bEchoOnly )
{
	QuotationRecover		oDataRecover;

	if( 0 != oDataRecover.OpenFile( sFilePath.c_str(), Configuration::GetConfig().GetBroadcastBeginTime() ) )
	{
		QuoCollector::GetCollector()->OnLog( TLV_ERROR, "CTPQuotation::LoadDataFile() : failed 2 open snap file : %s", sFilePath.c_str() );
		return -1;
	}

	if( true == bEchoOnly )
	{
		::printf( "合约代码,交易日,交易所代码,合约在交易所的代码,最新价,上次结算价,昨收盘,昨持仓量,今开盘,最高价,最低价,成交数量,成交金额,持仓量,今收盘,本次结算价,涨停板价,跌停板价,昨虚实度,今虚实度,最后修改时间,最后修改毫秒,\
申买价一,申买量一,申卖价一,申卖量一,申买价二,申买量二,申卖价二,申卖量二,申买价三,申买量三,申卖价三,申卖量三,申买价四,申买量四,申卖价四,申卖量四,申买价五,申买量五,申卖价五,申卖量五,当日均价,业务日期\n" );
	}

	while( true )
	{
		CThostFtdcDepthMarketDataField	oData = { 0 };

		if( oDataRecover.Read( (char*)&oData, sizeof(CThostFtdcDepthMarketDataField) ) <= 0 )
		{
			break;
		}

		if( true == bEchoOnly )
		{
			::printf( "%s,%s,%s,%s,%f,%f,%f,%f,%f,%f,%f,%d,%f,%f,%f,%f,%f,%f,%f,%f,%s,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%d,%f,%s\n", oData.InstrumentID, oData.TradingDay, oData.ExchangeID, oData.ExchangeInstID
					, oData.LastPrice, oData.PreSettlementPrice, oData.PreClosePrice, oData.PreOpenInterest, oData.OpenPrice, oData.HighestPrice, oData.LowestPrice, oData.Volume, oData.Turnover
					, oData.OpenInterest, oData.ClosePrice, oData.SettlementPrice, oData.UpperLimitPrice, oData.LowerLimitPrice, oData.PreDelta, oData.CurrDelta, oData.UpdateTime, oData.UpdateMillisec
					, oData.BidPrice1, oData.BidVolume1, oData.AskPrice1, oData.AskVolume1, oData.BidPrice2, oData.BidVolume2, oData.AskPrice2, oData.AskVolume2, oData.BidPrice3, oData.BidVolume3
					, oData.AskPrice3, oData.AskVolume3, oData.BidPrice4, oData.BidVolume4, oData.AskPrice4, oData.AskVolume4, oData.BidPrice5, oData.BidVolume5, oData.AskPrice5, oData.AskVolume5
					, oData.AveragePrice, oData.ActionDay );
		}
		else
		{
			OnRtnDepthMarketData( &oData );
		}
	}

	QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::LoadDataFile() : End of Quotation File..." );

	return 0;
}

int CTPQuotation::Execute()
{
	return LoadDataFile( Configuration::GetConfig().GetQuotationFilePath().c_str(), false );
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

		if( nRet > 0 )
		{
			QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::SubscribeQuotation() : Creating ctp session\'s dump file ......" );

			char			pszTmpFile[1024] = { 0 };			///< 准备行情数据落盘
			::sprintf( pszTmpFile, "Quotation_%u_%d.dmp", DateTime::Now().DateToLong(), DateTime::Now().TimeToLong() );
			int				nRet = m_oDataRecorder.OpenFile( JoinPath( Configuration::GetConfig().GetDumpFolder(), pszTmpFile ).c_str(), false );

			if( nRet == 0 )
			{
				QuoCollector::GetCollector()->OnLog( TLV_INFO, "CTPQuotation::SubscribeQuotation() : dump file created, result = %d", nRet );
			}
			else
			{
				QuoCollector::GetCollector()->OnLog( TLV_ERROR, "CTPQuotation::SubscribeQuotation() : cannot generate dump file, errorcode = %d", nRet );
			}
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

	m_nCodeCount = 0;
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

unsigned int CTPQuotation::GetCodeCount()
{
	CriticalLock	section( m_oLock );

	return m_nCodeCount;
}

void CTPQuotation::OnRtnDepthMarketData( CThostFtdcDepthMarketDataField *pMarketData )
{
	if( NULL == pMarketData )
	{
		QuoCollector::GetCollector()->OnLog( TLV_ERROR, "CTPQuotation::OnRtnDepthMarketData() : invalid market data pointer (NULL)" );
		return;
	}

	if( false == Configuration::GetConfig().IsBroadcastModel() )
	{
		m_oDataRecorder.Record( (char*)pMarketData, sizeof(CThostFtdcDepthMarketDataField) );
	}

	///< 判断是否收完全幅快照(以收到的代码是否有重复为判断依据)
	bool	bInitializing = (enum E_SS_Status)m_oWorkStatus != ET_SS_WORKING;
	if( true == bInitializing )
	{
		CriticalLock	section( m_oLock );

		if( m_setRecvCode.find( pMarketData->InstrumentID ) == m_setRecvCode.end() )
		{
			m_setRecvCode.insert( pMarketData->InstrumentID );
			m_nCodeCount = m_setRecvCode.size();
		}
		else
		{
			m_oWorkStatus = ET_SS_WORKING;	///< 收到重复代码，全幅快照已收完整
			bInitializing = false;
			m_oDataRecorder.Flush();
		}
	}

	FlushQuotation( pMarketData, bInitializing );
}

void CTPQuotation::FlushQuotation( CThostFtdcDepthMarketDataField* pQuotationData, bool bInitialize )
{
	double							dRate = 1.;				///< 放大倍数
	int								nSerial = 0;			///< 商品在码表的索引值
	tagDLFutureReferenceData_LF103	tagName = { 0 };		///< 商品基础信息结构
	tagDLFutureSnapData_HF105		tagSnapHF = { 0 };		///< 高速行情快照
	tagDLFutureSnapData_LF104		tagSnapLF = { 0 };		///< 低速行情快照
	tagDLFutureSnapBuySell_HF106	tagSnapBS = { 0 };		///< 档位信息
	tagDLFutureMarketStatus_HF102	tagStatus = { 0 };		///< 市场状态信息
	unsigned int					nSnapTradingDate = 0;	///< 快照交易日期

	::strncpy( tagName.Code, pQuotationData->InstrumentID, sizeof(tagName.Code) );
	::memcpy( tagSnapHF.Code, pQuotationData->InstrumentID, sizeof(tagSnapHF.Code) );
	::memcpy( tagSnapLF.Code, pQuotationData->InstrumentID, sizeof(tagSnapLF.Code) );
	::memcpy( tagSnapBS.Code, pQuotationData->InstrumentID, sizeof(tagSnapBS.Code) );

	if( (nSerial=QuoCollector::GetCollector()->OnQuery( 103, (char*)&tagName, sizeof(tagName) )) <= 0 )
	{
		return;
	}

	dRate = CTPQuoImage::GetRate( tagName.Kind );

	if( true == bInitialize ) {	///< 初始化行情
		tagSnapLF.PreOpenInterest = pQuotationData->PreOpenInterest*dRate+0.5;
	}
	if( pQuotationData->UpperLimitPrice > 0 ) {
		tagSnapLF.UpperPrice = pQuotationData->UpperLimitPrice*dRate+0.5;
	}
	if( pQuotationData->LowerLimitPrice > 0 ) {
		tagSnapLF.LowerPrice = pQuotationData->LowerLimitPrice*dRate+0.5;
	}

	tagSnapLF.Open = pQuotationData->OpenPrice*dRate+0.5;
	tagSnapLF.Close = pQuotationData->ClosePrice*dRate+0.5;
	tagSnapLF.PreClose = pQuotationData->PreClosePrice*dRate+0.5;
	tagSnapLF.PreSettlePrice = pQuotationData->PreSettlementPrice*dRate+0.5;
	tagSnapLF.SettlePrice = pQuotationData->SettlementPrice*dRate+0.5;
	tagSnapLF.PreOpenInterest = pQuotationData->PreOpenInterest;

	tagSnapHF.High = pQuotationData->HighestPrice*dRate+0.5;
	tagSnapHF.Low = pQuotationData->LowestPrice*dRate+0.5;
	tagSnapHF.Now = pQuotationData->LastPrice*dRate+0.5;
	tagSnapHF.Position = pQuotationData->OpenInterest;
	tagSnapHF.Volume = pQuotationData->Volume;

//	if( EV_MK_ZZ == eMkID )		///< 郑州市场的成交金额特殊处理： = 金额 * 合约单位
//		tagSnapHF.Amount = pQuotationData->Turnover * refNameTable.ContractMult;

	tagSnapHF.Amount = pQuotationData->Turnover;
	tagSnapBS.Buy[0].Price = pQuotationData->BidPrice1*dRate+0.5;
	tagSnapBS.Buy[0].Volume = pQuotationData->BidVolume1;
	tagSnapBS.Sell[0].Price = pQuotationData->AskPrice1*dRate+0.5;
	tagSnapBS.Sell[0].Volume = pQuotationData->AskVolume1;
	tagSnapBS.Buy[1].Price = pQuotationData->BidPrice2*dRate+0.5;
	tagSnapBS.Buy[1].Volume = pQuotationData->BidVolume2;
	tagSnapBS.Sell[1].Price = pQuotationData->AskPrice2*dRate+0.5;
	tagSnapBS.Sell[1].Volume = pQuotationData->AskVolume2;
	tagSnapBS.Buy[2].Price = pQuotationData->BidPrice3*dRate+0.5;
	tagSnapBS.Buy[2].Volume = pQuotationData->BidVolume3;
	tagSnapBS.Sell[2].Price = pQuotationData->AskPrice3*dRate+0.5;
	tagSnapBS.Sell[2].Volume = pQuotationData->AskVolume3;
	tagSnapBS.Buy[3].Price = pQuotationData->BidPrice4*dRate+0.5;
	tagSnapBS.Buy[3].Volume = pQuotationData->BidVolume4;
	tagSnapBS.Sell[3].Price = pQuotationData->AskPrice4*dRate+0.5;
	tagSnapBS.Sell[3].Volume = pQuotationData->AskVolume4;
	tagSnapBS.Buy[4].Price = pQuotationData->BidPrice5*dRate+0.5;
	tagSnapBS.Buy[4].Volume = pQuotationData->BidVolume5;
	tagSnapBS.Sell[4].Price = pQuotationData->AskPrice5*dRate+0.5;
	tagSnapBS.Sell[4].Volume = pQuotationData->AskVolume5;

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
		tagStatus.MarketStatus = 1;
		tagStatus.MarketTime = nSnapUpdateTime;
		QuoCollector::GetCollector()->OnData( 102, (char*)&tagStatus, sizeof(tagStatus), true );
	}

	QuoCollector::GetCollector()->OnData( 104, (char*)&tagSnapLF, sizeof(tagSnapLF), true );
	QuoCollector::GetCollector()->OnData( 105, (char*)&tagSnapHF, sizeof(tagSnapHF), true );
	QuoCollector::GetCollector()->OnData( 106, (char*)&tagSnapBS, sizeof(tagSnapBS), true );
}

void CTPQuotation::OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	QuoCollector::GetCollector()->OnLog( TLV_ERROR, "CTPQuotation::OnRspError() : [%s] ErrorCode=[%d], ErrorMsg=[%s],RequestID=[%d], Chain=[%d]"
										, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );
}






