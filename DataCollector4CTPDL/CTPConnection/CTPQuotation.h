#ifndef __CTP_SESSION_H__
#define __CTP_SESSION_H__


#pragma warning(disable:4786)
#include <set>
#include <string>
#include <stdexcept>
#include "ThostFtdcMdApi.h"
#include "../Configuration.h"
#include "../CTP_DL_QuoProtocal.h"
#include "../Infrastructure/Lock.h"


/**
 * @class			CTPWorkStatus
 * @brief			CTP工作状态管理
 * @author			barry
 */
class CTPWorkStatus
{
public:
	/**
	 * @class	E_SS_Status
	 * @brief	CTP行情会话状态枚举
	 */
    enum E_SS_Status
	{
		ET_SS_UNACTIVE = 0,				///< 未激活:	需要对Session调用Initialize()
		ET_SS_DISCONNECTED,				///< 断开状态
		ET_SS_CONNECTED,				///< 连通状态
		ET_SS_LOGIN,					///< 登录成功
        ET_SS_INITIALIZING,				///< 初始化码表/快照中
		ET_SS_WORKING,					///< 正常工作中
    };

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
	enum E_SS_Status	m_eWorkStatus;			///< 行情工作状态
};


typedef	std::set<std::string>		T_SET_INSTRUMENTID;			///< 商品代码集合


/**
 * @class			CTPQuotation
 * @brief			CTP会话管理对象
 * @detail			封装了针对商品期货期权各市场的初始化、管理控制等方面的方法
 * @author			barry
 */
class CTPQuotation : public CThostFtdcMdSpi
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
public:///< 公共方法函数
	/**
	 * @brief			获取会话状态信息
	 */
	CTPWorkStatus&		GetWorkStatus();

	/**
	 * @brief			发送登录请求包
	 */
    void				SendLoginRequest();

	/**
	 * @brief			取得收到全幅快照的商品数量(即初始化成功的)
	 * @return			初始化成功的商品数量
	 * @note			如果未收全，则返回0
	 */
	unsigned int		GetInitialCodeNum();

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
	CriticalObject		m_oLock;				///< 临界区对象
};




#endif






