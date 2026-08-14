#ifndef CHUANGXINCAN_H
#define CHUANGXINCAN_H

#include <QObject>
#include <QThread>
#include <QLibrary>
#include <QMutex>
#include "canbase.h"
#include "zlgcan.h"
//marked,cause reflict defination interface func with zlgcan.h
//also need use zlgs' func when zlg can adapt plug in.
//#include <ControlCANFD.h>


class ChuangXinCan : public QThread
{
  Q_OBJECT

public:
  ChuangXinCan();

  void stop();
  bool openCanDevice(uint8_t canIdx);
  void closeCanDevice();
  void sendCanData(quint32 canID, const QByteArray &data);
  void receiveCanData(QByteArray& RecvBuf);
  void setFilterCanId(quint32 id);

private:
  void run();
  void sleep(unsigned int msec);

private:
  QLibrary m_ControlCANFD_DLL;

  typedef DEVICE_HANDLE FUNC_CALL (*OpenDevice)(UINT device_type, UINT device_index, UINT reserved);
  typedef UINT FUNC_CALL (*CloseDevice)(DEVICE_HANDLE device_handle);

  typedef UINT FUNC_CALL (*TransmitFD)(CHANNEL_HANDLE channel_handle, ZCAN_TransmitFD_Data* pTransmit, UINT len);

  typedef UINT FUNC_CALL (*ZCAN_GetRecvNum)(CHANNEL_HANDLE channel_handle,BYTE type);  // type:TYPE_CAN, TYPE_CANFD, TYPE_ALL_DATA
  typedef UINT FUNC_CALL (*ReceiveFD)(CHANNEL_HANDLE channel_handle, ZCAN_ReceiveFD_Data* pReceive, UINT len, int wait_time);

  typedef IProperty* FUNC_CALL (*getIProperty)(DEVICE_HANDLE device_handle);
  typedef UINT FUNC_CALL (*ReleaseIProperty)(IProperty * pIProperty);

  typedef CHANNEL_HANDLE FUNC_CALL (*InitCAN)(DEVICE_HANDLE device_handle, UINT can_index, ZCAN_CHANNEL_INIT_CONFIG* pInitConfig);
  typedef UINT FUNC_CALL (*StartCAN)(CHANNEL_HANDLE channel_handle);
  typedef UINT FUNC_CALL (*ResetCAN)(CHANNEL_HANDLE channel_handle);

signals:
  void updateSendStatus(unsigned int sendCnt, QByteArray currentFrameData);
  void recvedCANFDData(QByteArray recvCANFDData);


private:
  void* canDevHdl;
  void* canCHNHdl;
  bool canOpenSt;
  bool stopped;
  quint32 m_CANID;
  QMutex m_idMutex;
  CloseDevice m_CloseDev;
  OpenDevice m_CANOpenDev;
  getIProperty m_GetIProp;
  InitCAN m_initCAN;
  ResetCAN m_resetCAN;
  StartCAN m_startCAN;
  ZCAN_GetRecvNum m_RecvNum;
  ReceiveFD m_recvFD;
  ResetCAN m_reset_CAN;
  TransmitFD m_transmitFD;
  ReleaseIProperty m_releaseProperty;

  IProperty* m_devPtr;
  ZCAN_CHANNEL_INIT_CONFIG m_config;

};

#endif // CHUANGXINCAN_H
