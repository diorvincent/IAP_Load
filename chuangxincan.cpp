#include "chuangxincan.h"
#include <QApplication>
#include <QMessageBox>
#include <QtDebug>
#include <QDateTime>

ChuangXinCan::ChuangXinCan()
{
  QString str1 = QApplication::applicationDirPath();
  QString str(str1+"/ControlCANFD.dll" );
  m_ControlCANFD_DLL.setFileName(str);
  if(!m_ControlCANFD_DLL.load())
  {
    QMessageBox::critical(NULL, QStringLiteral("错误"),QStringLiteral("库文件ControlCANFD.dll加载失败"));
    return;
  }

  m_CloseDev      = (CloseDevice)m_ControlCANFD_DLL.resolve("ZCAN_CloseDevice");
  m_CANOpenDev    = (OpenDevice)m_ControlCANFD_DLL.resolve("ZCAN_OpenDevice");
  m_GetIProp      = (getIProperty)m_ControlCANFD_DLL.resolve("GetIProperty");
  m_initCAN       = (InitCAN)m_ControlCANFD_DLL.resolve("ZCAN_InitCAN");
  m_resetCAN      = (ResetCAN)m_ControlCANFD_DLL.resolve("ZCAN_ResetCAN");
  m_startCAN      = (StartCAN)m_ControlCANFD_DLL.resolve("ZCAN_StartCAN");
  m_recvFD        = (ReceiveFD)m_ControlCANFD_DLL.resolve("ZCAN_ReceiveFD");
  m_RecvNum       = (ZCAN_GetRecvNum)m_ControlCANFD_DLL.resolve("ZCAN_GetReceiveNum");
  m_transmitFD    = (TransmitFD)m_ControlCANFD_DLL.resolve("ZCAN_TransmitFD");
  m_releaseProperty =(ReleaseIProperty)m_ControlCANFD_DLL.resolve("ReleaseIProperty");

  m_devPtr = nullptr;
  memset(&m_config,0,sizeof(m_config));

  stopped = false;
  canOpenSt = false;
  canDevHdl = INVALID_DEVICE_HANDLE;
  canCHNHdl = INVALID_CHANNEL_HANDLE;

}

bool ChuangXinCan::openCanDevice(uint8_t canIdx)
{
  if (canDevHdl != INVALID_DEVICE_HANDLE) {
      m_CloseDev(canDevHdl);
  }

  canDevHdl = m_CANOpenDev(canIdx+41, canIdx, 0);
  if(canDevHdl != INVALID_DEVICE_HANDLE)
  {
      m_devPtr = m_GetIProp(canDevHdl);
      if(m_devPtr == nullptr)//Failed to set the specified path property
      {
          m_CloseDev(canDevHdl);
          return false;
      }
      //set CANFD standard(0:ISO; 1:Bosch)
      m_devPtr->SetValue("0/canfd_standard", QString::number(canIdx).toStdString().c_str()); //channel0 is ISO standard

      char strParam[24];
      memset(strParam, 0, sizeof(strParam));
      // 仲裁段波特：1Mbps
      sprintf(strParam, "%d/canfd_abit_baud_rate", canIdx);
      if(m_devPtr->SetValue(strParam, "1000000")  != STATUS_OK){
          qDebug() << "set baudrate 1M failure.";
      }

      // 数据段波特：5Mbps
      memset(strParam, 0, sizeof(strParam));
      sprintf(strParam, "%d/canfd_dbit_baud_rate", canIdx);
      if (m_devPtr->SetValue(strParam, "5000000") != STATUS_OK) {
          qDebug() << "set baudrate 5M failure.";
      }

      //初始化通道
      ZCAN_CHANNEL_INIT_CONFIG canInit;
      memset(&canInit, 0, sizeof(canInit));
      canInit.can_type = TYPE_CANFD;
      canInit.canfd.mode = 0;
      canInit.canfd.acc_code = 0;
      canInit.canfd.acc_mask = 0x000000FF;
      canInit.canfd.filter = 0;

      canCHNHdl = m_initCAN(canDevHdl, canIdx, &canInit);
      if (canCHNHdl == INVALID_CHANNEL_HANDLE) {
          qDebug() << "Init channel failured:" << canCHNHdl ;
          m_CloseDev(canDevHdl);
          canDevHdl = INVALID_DEVICE_HANDLE;
          return false;
      }

      //设置终端电阻
      sprintf(strParam, "%d/initenal_resistance", canIdx);
      if(STATUS_OK != m_devPtr->SetValue(strParam, QString::number(1).toStdString().c_str()))
      {
         qDebug() << "terminal resistor enable failured, return code:" << STATUS_ERR;
         QMessageBox::critical(nullptr, "提示", "终端电阻启用失败，可能影响通信！");
      } else {
         qDebug() << "terminal resistor enable succeed (chn:" << canIdx << ")";
      }

      //关闭属性设置
      if(m_releaseProperty(m_devPtr) != STATUS_OK)
        {
          qDebug() << "Release property failured, return code:" << STATUS_ERR;
        }

      //开启CAN
      if (m_startCAN(canCHNHdl) != STATUS_OK)
      {
          QMessageBox::critical(nullptr, "CANFD错误", "通道启动失败！");
          qDebug() << "enable failured(CHN:" << canIdx << ")";

          m_resetCAN(canCHNHdl);
          m_CloseDev(canDevHdl);

          canCHNHdl = INVALID_CHANNEL_HANDLE;
          canDevHdl = INVALID_DEVICE_HANDLE;

          return false;
      }
      canOpenSt=true;
  }
  else
  {
      QMessageBox::critical(nullptr, "CANFD错误", "打开设备失败！");
      return false;
  }

  return true;
}

void ChuangXinCan::closeCanDevice()
{
  if (canCHNHdl != INVALID_CHANNEL_HANDLE) {
      m_resetCAN(canCHNHdl);
  }

  if (canDevHdl != INVALID_DEVICE_HANDLE) {
      m_CloseDev(canDevHdl);
  }
  canOpenSt = false;
}

void ChuangXinCan::sendCanData(quint32 canID, const QByteArray &data)
{
  // CAN FD 单帧最大 64 字节
  const int MAX_FD_DATA_LEN = 64;
  int dataPos = 0;
  int totalLen = data.size();

  setFilterCanId(canID); // 设置过滤ID（加锁)
  while (dataPos < totalLen)
  {
      int currentFrameLen = qMin(MAX_FD_DATA_LEN, totalLen - dataPos);
      QByteArray currentFrameData = data.mid(dataPos, currentFrameLen);

      canfd_frame fdFrame;
      memset(&fdFrame, 0, sizeof(canfd_frame));
      fdFrame.can_id = MAKE_CAN_ID(canID, 0, 0, 0);
      fdFrame.len = currentFrameLen;
      fdFrame.flags = 0x00; //CANFD_BRS

      memcpy(fdFrame.data, currentFrameData.data(), currentFrameLen);

      ZCAN_TransmitFD_Data txFdData;
      memset(&txFdData, 0, sizeof(txFdData));
      txFdData.frame = fdFrame;
      txFdData.frame.flags = 0;
      txFdData.transmit_type = 0; // 1=single send
      dataPos += currentFrameLen;

      UINT sendCnt = m_transmitFD(canCHNHdl, &txFdData, 1);
      emit updateSendStatus(sendCnt, currentFrameData);
  }
}

void ChuangXinCan::receiveCanData(QByteArray& RecvBuf)
{
  if (!canOpenSt || canCHNHdl == INVALID_CHANNEL_HANDLE)
  {
      return;
  }
  UINT frameCount = 0;
  frameCount = m_RecvNum(canCHNHdl, TYPE_CANFD);

  if(frameCount > 0)
  {
      ZCAN_ReceiveFD_Data rxFdData[1];
      UINT recvCnt = m_recvFD(canCHNHdl, rxFdData, 1, 1);
      if (recvCnt > 0)
      {
          for (UINT i = 0; i < recvCnt; i++)
          {
              canfd_frame rxFrame = rxFdData[i].frame;
              bool isExtFrame = (rxFrame.can_id & CAN_EFF_FLAG) != 0;
              quint32 recvPureId = rxFrame.can_id & CAN_EFF_MASK;

    #ifdef QT_DEBUG
              QString frameType = isExtFrame ? "ExtendFrm" : "StandardFrm";
              qDebug() << "FrameTyp：" << frameType << "  OriginalID：0x" << QString::number(recvPureId, 16);
    #endif
              // 只处理标准帧 + 匹配目标ID
              if (!isExtFrame && recvPureId == m_CANID)
              {
                  QByteArray recvBuf(reinterpret_cast<const char*>(rxFrame.data), rxFrame.len);
                  RecvBuf.append(recvBuf);
    #ifdef QT_DEBUG
                  static QTime RecvTime = QTime::currentTime();
                  QString strTime = RecvTime.toString("mm:ss:zzz");
                  qDebug() << strTime << " " << recvBuf.toHex();
    #endif
              }
          }
      }

  }


}

void ChuangXinCan::stop()
{
  stopped = true;
}

void ChuangXinCan::setFilterCanId(quint32 id)
{
  QMutexLocker locker(&m_idMutex);
  m_CANID = id;
}

void ChuangXinCan::run()
{
  quint32 targetId;
  if (!canOpenSt || canCHNHdl == INVALID_CHANNEL_HANDLE)
  {
     return;
  }

  while(!stopped)
  {
      UINT frameCount = 0;
      frameCount = m_RecvNum(canCHNHdl, TYPE_CANFD);

      if(frameCount > 0)
      {
          ZCAN_ReceiveFD_Data rxFdData[8];
          UINT recvCnt = m_recvFD(canCHNHdl, rxFdData, 8, 10);
          if (recvCnt > 0)
          {
              for (UINT i = 0; i < recvCnt; i++)
              {
                  canfd_frame rxFrame = rxFdData[i].frame;
                  bool isExtFrame = (rxFrame.can_id & CAN_EFF_FLAG) != 0;
                  quint32 recvPureId = rxFrame.can_id & CAN_EFF_MASK;

#ifdef QT_DEBUG
                  QString frameType = isExtFrame ? "ExtendFrm" : "StandardFrm";
                  qDebug() << "FrameTyp：" << frameType << "  OriginalID：0x" << QString::number(recvPureId, 16);
#endif
                  // 只处理标准帧 + 匹配目标ID
                  QMutexLocker locker(&m_idMutex);
                  targetId = m_CANID;
                  if (!isExtFrame && recvPureId == targetId)
                  {
                      //只把当前这一CAN帧的数据抛出去！
                      QByteArray oneCanFrameData(reinterpret_cast<const char*>(rxFrame.data), rxFrame.len);
                      emit recvedCANFDData(oneCanFrameData);

#ifdef QT_DEBUG
                      static QTime RecvTime = QTime::currentTime();
                      QString strTime = RecvTime.toString("mm:ss:zzz");
                      qDebug() << strTime << " " << oneCanFrameData.toHex();
#endif
                  }
              }
          }
          msleep(1);
      }
  }
  stopped = false;
}


void ChuangXinCan::sleep(unsigned int msec)
{
   QTime dieTime = QTime::currentTime().addMSecs(msec);
   while( QTime::currentTime() < dieTime )
       QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}
