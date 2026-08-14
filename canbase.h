#ifndef CANBASE_H
#define CANBASE_H

#include <QObject>
#include <QByteArray>

typedef void *DEVICE_HANDLE;
typedef void *CHANNEL_HANDLE;

class canBase :  public QObject
{
  Q_OBJECT

public:
  canBase();
  ~canBase();

  virtual bool openCanDevice(uint8_t canIdx)=0;
  virtual void closeCanDevice()=0;
  virtual void sendCanData(quint32 canID, const QByteArray &data)=0;
  virtual void receiveCanData(QByteArray& RecvBuf)=0;
  virtual void stop()=0;

public:
  bool getCANStatus(){return m_canOpenStatus;}
  void setCANStatus(bool bOpened){m_canOpenStatus = bOpened;}
  quint32 getCANID(){return m_canId;}
  void setCANID(quint32 canID){m_canId = canID;}
  void* getDevHandle(){return m_canDeviceHandle;}
  void setDevHandle(DEVICE_HANDLE canDeviceHandle){m_canDeviceHandle = canDeviceHandle;}
  void* getCHNHandle(){return m_canChannelHandle;}
  void setChnHandle(CHANNEL_HANDLE canChannelHandle){m_canChannelHandle = canChannelHandle;}


signals:
  void updateSendStatus(unsigned int sendCnt, QByteArray currentFrameData);

private:
  DEVICE_HANDLE m_canDeviceHandle;
  CHANNEL_HANDLE m_canChannelHandle;
  bool m_canOpenStatus;
  quint32 m_canId;
};

#endif // CANBASE_H
