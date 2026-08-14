#ifndef ZGLCAN_H
#define ZGLCAN_H

#include <QMap>
#include <QValidator>
#include "canbase.h"
#include "typedef.h"
#include "config.h"
#include "zlgcan.h"
#include "canframe.h"


class ZLGCan_s : public canBase
{
  Q_OBJECT

public:
  ZLGCan_s();
  ~ZLGCan_s();

  virtual bool openCanDevice(uint8_t canIdx);
  virtual void closeCanDevice();
  virtual void sendCanData(quint32 canID, const QByteArray &data);
  virtual void receiveCanData(QByteArray& RecvBuf);
  virtual void stop(){};

};

#endif // ZGLCAN_H
