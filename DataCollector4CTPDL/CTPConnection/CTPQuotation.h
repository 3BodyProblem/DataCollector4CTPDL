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
 * @brief							数据包队列缓存
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
	 * @brief						初始化缓存对象
	 * @param[in]					nMaxBufSize				将分配的缓存大小
	 * @return						==0						成功
	 */
	int								Initialize( unsigned long nMaxBufSize );

	/**
	 * @brief						释放缓存空间
	 */
	void							Release();

public:
	/**
	 * @brief						存储数据
	 * @param[in]					nDataID					数据ID
	 * @param[in]					pData					数据指针
	 * @param[in]					nDataSize				数据长度
	 * @param[in]					nSeqNo					当前数据块的更新序号
	 * @return						==0						成功
	 * @note						当nDataID不等于前一个包的nDataID时，将新启用一个Package封装
	 */
	int								PushBlock( unsigned int nDataID, const char* pData, unsigned int nDataSize, unsigned __int64 nSeqNo, unsigned int& nMsgCount, unsigned int& nBodySize );

	/**
	 * @brief						获取一个数据包
	 * @param[out]					pBuff					输出数据缓存地址
	 * @param[in]					nBuffSize				数据缓存长度
	 * @param[out]					nMsgID					数据消息ID
	 * @return						>0						数据长度
									==0						无数据
									<0						出错
	 */
	int								GetOnePkg( char* pBuff, unsigned int nBuffSize, unsigned int& nMsgID );

	/**
	 * @brief						是否为空
	 * @return						true					为空
	 */
	bool							IsEmpty();

	/**
	 * @brief						获取余下空间的百分比
	 */
	float							GetPercentOfFreeSize();

protected:
	CriticalObject					m_oLock;				///< 锁
	char*							m_pPkgBuffer;			///< 数据包缓存地址
	unsigned int					m_nMaxPkgBufSize;		///< 数据包缓存大小
	unsigned int					m_nCurrentPkgHeadPos;	///< 当前起始位置(当前所写入包的包头)
	unsigned int					m_nFirstPkgHeadPos;		///< 起始位置索引(在写，或已经写完包的包头，即未发送数据的头部)
	unsigned int					m_nCurrentWritePos;		///< 结束位置索引(正在写入的位置)
};



/**
 * @class			CTPWorkStatus
 * @brief			CTP工作状态管理
 * @author			barry
 */
class CTPWorkStatus
{
public:
	/**
	 * @brief				应状态值映射成状态字符串
	 */
	static	std::string&	CastStatusStr( enum E_SS_Status eStatus );

public:
	/**
	 * @brief			构造
	 * @param			eMkID			市场编号
	 */
	CTPWorkStatus();
	CTPWorkStatus( const CTPWorkStatus& refStatus );

	/**
	 * @brief			赋值重载
						每次值变化，将记录日志
	 */
	CTPWorkStatus&		operator= ( enum E_SS_Status eWorkStatus );

	/**
	 * @brief			重载转换符
	 */
	operator			enum E_SS_Status();

private:
	CriticalObject		m_oLock;				///< 临界区对象
	enum E_SS_Status	m_eWorkStatus;			///< 行情工作状态
};


typedef	std::set<std::string>		T_SET_INSTRUMENTID;			///< 商品代码集合


/**
 * @class			CTPQuotation
 * @brief			CTP会话管理对象
 * @detail			封装了针对商品期货期权各市场的初始化、管理控制等方面的方法
 * @author			barry
 */
class CTPQuotation : public CThostFtdcMdSpi, public SimpleTask
{
public:
	CTPQuotation();
	~CTPQuotation();

	/**
	 * @brief			释放ctp行情接口
	 */
	int					Destroy();

	/**
	 * @brief			初始化ctp行情接口
	 * @return			>=0			成功
						<0			错误
	 * @note			整个对象的生命过程中，只会启动时真实的调用一次
	 */
	int					Activate() throw(std::runtime_error);

protected:
	/**
	 * @brief			根据新的码表重新订阅行情
	 * @return			>=0			成功
	 */
	int					SubscribeQuotation();

	/**
	 * @brief			任务函数(内循环)
	 * @return			==0					成功
						!=0					失败
	 */
	int					Execute();

public:///< 公共方法函数
	/**
	 * @brief			加载行情数据
	 * @param[in]		sFilePath			文件路径
	 * @param[in]		bEchoOnly			回显标识
	 * @return			>=0					成功
	 */
	int					LoadDataFile( std::string sFilePath, bool bEchoOnly = false );

	/**
	 * @brief			获取会话状态信息
	 */
	CTPWorkStatus&		GetWorkStatus();

	/**
	 * @brief			获取代码数量
	 */
	unsigned int		GetCodeCount();

	/**
	 * @brief			发送登录请求包
	 */
    void				SendLoginRequest();

	/** 
	 * @brief			刷数据到发送缓存
	 * @param[in]		pQuotationData			行情数据结构
	 * @param[in]		bInitialize				初始化数据的标识
	 */
	void				FlushQuotation( CThostFtdcDepthMarketDataField* pQuotationData, bool bInitialize );

public:///< CThostFtdcMdSpi的回调接口
	void OnFrontConnected();
	void OnFrontDisconnected( int nReason );
	void OnHeartBeatWarning( int nTimeLapse );
	void OnRspUserLogin( CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );
	void OnRtnDepthMarketData( CThostFtdcDepthMarketDataField *pMarketData );
	void OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );

private:
	CThostFtdcMdApi*	m_pCTPApi;				///< 第三方提供的cffex接口
	CTPWorkStatus		m_oWorkStatus;			///< 工作状态
	T_SET_INSTRUMENTID	m_setRecvCode;			///< 收到的代码集合
	unsigned int		m_nCodeCount;			///< 收到的快照商品数量
	CriticalObject		m_oLock;				///< 临界区对象
protected:
	QuotationRecorder	m_oDataRecorder;		///< 行情落盘对象
};




#endif






