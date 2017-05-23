#ifndef __DATA_TABLE_PAINTER_H__
#define	__DATA_TABLE_PAINTER_H__


#include "Interface.h"
#include "Configuration.h"
#include "Infrastructure/Lock.h"
#include "CTPConnection/CTPQuoImage.h"
#include "CTPConnection/CTPQuotation.h"


/**
 * @class						T_LOG_LEVEL
 * @brief						��־����
 * @author						barry
 */
enum T_LOG_LEVEL
{
	TLV_INFO = 0,
	TLV_WARN = 1,
	TLV_ERROR = 2,
	TLV_DETAIL = 3,
};


/**
 * @class						QuoCollector
 * @brief						�������ݲɼ�ģ��������
 * @date						2017/5/15
 * @author						barry
 */
class QuoCollector : public SimpleTask
{
protected:
	QuoCollector();

public:
	/**
	 * @brief					��ȡ���ݲɼ�����ĵ�������
	 */
	static QuoCollector&		GetCollector();

	/**
	 * @brief					��ʼ�����ݲɼ���
	 * @param[in]				pIDataHandle				����ص��ӿ�
	 * @return					==0							��ʼ���ɹ�
								!=0							����
	 */
	int							Initialize( I_DataHandle* pIDataHandle );

	/**
	 * @brief					�ͷ���Դ
	 */
	void						Release();

public:
	/**
	 * @brief					���ط�������ص��ӿڵ�ַ
	 * @return					����ص��ӿ�ָ���ַ
	 */
	I_DataHandle*				operator->();

	/**
	 * @brief					�ӱ����ļ�������˿����»ָ�����������������
	 * @return					==0							�ɹ�
								!=0							����
	 */
	int							RecoverQuotation();

	/**
	 * @brief					ȡ�òɼ�ģ��ĵ�ǰ״̬
	 */
	enum E_SS_Status			GetCollectorStatus();

	/**
	 * @brief					ȡ���ܶ��ĵ���Ʒ
	 * @param[out]				pszCodeList	��Ʒ�б�
	 * @param[in]				nListSize	��Ʒ�б���
	 * @return					���ظ���
	 */
	int							GetSubscribeCodeList( char (&pszCodeList)[1024*5][20], unsigned int nListSize );

protected:
	/**
	 * @brief					������(��ѭ��)
	 * @return					==0					�ɹ�
								!=0					ʧ��
	 */
	virtual int					Execute();

protected:
	I_DataHandle*				m_pCbDataHandle;			///< ����(����/��־�ص��ӿ�)
	CTPQuoImage					m_oImageData;				///< ������������������
	CTPQuotation				m_oQuotationData;			///< ʵʱ�������ݻỰ����
};





/**
 * @brief						DLL�����ӿ�
 * @author						barry
 * @date						2017/4/1
 */
extern "C"
{
	/**
	 * @brief								��ʼ�����ݲɼ�ģ��
	 */
	__declspec(dllexport) int __stdcall		Initialize( I_DataHandle* pIDataHandle );

	/**
	 * @brief								�ͷ����ݲɼ�ģ��
	 */
	__declspec(dllexport) void __stdcall	Release();

	/**
	 * @brief								���³�ʼ����������������
	 */
	__declspec(dllexport) int __stdcall		RecoverQuotation();

	/**
	 * @brief								��ȡģ��ĵ�ǰ״̬
	 */
	__declspec(dllexport) int __stdcall		GetStatus();

	/**
	 * @brief								��ȡ�г����
	 */
	__declspec(dllexport) int __stdcall		GetMarketID();

	/**
	 * @brief								��Ԫ���Ե�������
	 */
	__declspec(dllexport) void __stdcall	ExecuteUnitTest();
}




#endif





