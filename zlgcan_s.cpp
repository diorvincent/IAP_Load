#include "zlgcan_s.h"
#include <QMessageBox>
#include <QtDebug>
#include <QDateTime>
#include <QThread>

ZLGCan_s::ZLGCan_s()
{
  setDevHandle(INVALID_DEVICE_HANDLE);
  setChnHandle(INVALID_CHANNEL_HANDLE);
}
ZLGCan_s::~ZLGCan_s()
{

}

bool ZLGCan_s::openCanDevice(uint8_t canIdx)
{
  char path[24];
  memset(path, 0, sizeof(path));

  void* canDevHdl = INVALID_DEVICE_HANDLE;
  canDevHdl = getDevHandle();

  if (canDevHdl != INVALID_DEVICE_HANDLE) {
      ZCAN_CloseDevice(canDevHdl);
  }
  canDevHdl = ZCAN_OpenDevice(ZCAN_USBCANFD_200U, 0, 0);
  if (canDevHdl == INVALID_DEVICE_HANDLE) {
      QMessageBox::critical(nullptr, "CANFD错误", "打开设备失败！");
      return false;
  }
  setDevHandle(canDevHdl);

  int chn_idx = canIdx;
  qDebug() << "通道：" << chn_idx ;

  // 仲裁段波特：1Mbps
  sprintf(path, "%d/canfd_abit_baud_rate", chn_idx);
  if (ZCAN_SetValue(canDevHdl, path, "1000000") != STATUS_OK) {
      qDebug() << "设置仲裁1M波特失败";
  }
  // 数据段波特：5Mbps
  sprintf(path, "%d/canfd_dbit_baud_rate", chn_idx);
  if (ZCAN_SetValue(canDevHdl, path, "5000000") != STATUS_OK) {
      qDebug() << "设置数据5M波特失败";
  }

  ZCAN_CHANNEL_INIT_CONFIG canInit;
  memset(&canInit, 0, sizeof(canInit));
  canInit.can_type = TYPE_CANFD;
  canInit.canfd.mode = 0;
  canInit.canfd.acc_code = 0;
  canInit.canfd.acc_mask = 0x000000FF;
  canInit.canfd.filter = 0;

  void* canCHNHdl = INVALID_CHANNEL_HANDLE;
  canCHNHdl = ZCAN_InitCAN(canDevHdl, chn_idx, &canInit);
  if (canCHNHdl == INVALID_CHANNEL_HANDLE) {
      qDebug() << "初始化通道失败：" << canCHNHdl ;
      ZCAN_CloseDevice(canDevHdl);
      setDevHandle(INVALID_DEVICE_HANDLE);
      return false;
  }
  setChnHandle(canCHNHdl);

  sprintf(path, "%d/initenal_resistance", chn_idx);
  int res = ZCAN_SetValue(canDevHdl, path, "1");
  if (res != STATUS_OK) {
         qDebug() << "终端电阻启用失败，返回码：" << res;
         QMessageBox::critical(nullptr, "提示", "终端电阻启用失败，可能影响通信！");
     } else {
         qDebug() << "终端电阻启用成功（通道" << chn_idx << "）";
  }

  if (ZCAN_StartCAN(canCHNHdl) != STATUS_OK) {
      QMessageBox::critical(nullptr, "CANFD错误", "通道启动失败！");
      qDebug() << "启动失败（通道" << chn_idx << "）";
      ZCAN_ResetCAN(canCHNHdl);
      ZCAN_CloseDevice(canDevHdl);

      setChnHandle(INVALID_CHANNEL_HANDLE);
      setDevHandle(INVALID_DEVICE_HANDLE);

      return false;
  }
  setCANStatus(true);
  return true;
}

void ZLGCan_s::closeCanDevice()
{
  void* canDevHdl = INVALID_DEVICE_HANDLE;
  void* canCHNHdl = INVALID_CHANNEL_HANDLE;

  canDevHdl = getDevHandle();
  canCHNHdl = getCHNHandle();

  if (canCHNHdl != INVALID_CHANNEL_HANDLE) {
      ZCAN_ResetCAN(canCHNHdl);
      setChnHandle(canCHNHdl);
  }

  if (canDevHdl != INVALID_DEVICE_HANDLE) {
      ZCAN_CloseDevice(canDevHdl);
      setDevHandle(canDevHdl);
  }

  setCANStatus(false);
}

void ZLGCan_s::sendCanData(quint32 canID, const QByteArray &data)
{
  // CAN FD 单帧最大 64 字节
  const int MAX_FD_DATA_LEN = 64;
  int dataPos = 0;
  int totalLen = data.size();

  void* canCHNHdl = INVALID_CHANNEL_HANDLE;
  canCHNHdl = getCHNHandle();
  setCANID(canID);

  while (dataPos < totalLen) {
      int currentFrameLen = qMin(MAX_FD_DATA_LEN, totalLen - dataPos);
      QByteArray currentFrameData = data.mid(dataPos, currentFrameLen);

      canfd_frame fdFrame;
      memset(&fdFrame, 0, sizeof(canfd_frame));
      fdFrame.can_id = MAKE_CAN_ID(canID, 0, 0, 0);
      fdFrame.len = currentFrameLen;
      fdFrame.flags = 0x00;

      memcpy(fdFrame.data, currentFrameData.data(), currentFrameLen);

      ZCAN_TransmitFD_Data txFdData;
      memset(&txFdData, 0, sizeof(txFdData));
      txFdData.frame = fdFrame;
      txFdData.transmit_type = 0; // 0=正常发送
      dataPos += currentFrameLen;

      UINT sendCnt = ZCAN_TransmitFD(canCHNHdl, &txFdData, 1);
      if( sendCnt < 0)
        break;

      emit updateSendStatus(sendCnt, currentFrameData);
  }
}

void ZLGCan_s::receiveCanData(QByteArray& RecvBuf)
{
  bool canOpenSt = false;
  void* canCHNHdl = INVALID_CHANNEL_HANDLE;

  canOpenSt = getCANStatus();
  canCHNHdl = getCHNHandle();

  if (!canOpenSt || canCHNHdl == INVALID_CHANNEL_HANDLE)
  {
      return;
  }

  ZCAN_ReceiveFD_Data rxFdData[1];
  UINT recvCnt = ZCAN_ReceiveFD(canCHNHdl, rxFdData, 1, 1);
  //qDebug() << "【接收轮询】本次读到帧数：" << recvCnt;

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
          if (!isExtFrame && recvPureId == getCANID())
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
