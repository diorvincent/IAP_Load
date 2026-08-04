#ifndef DATABUFFER_H
#define DATABUFFER_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QMutex>
#include <QReadWriteLock>

class DataBuffer : public QObject
{
    Q_OBJECT
public:
    explicit DataBuffer(int maxSize = 10000, QObject *parent = nullptr);

    void append(double x, double y);
    void append(const QPointF &point);
    QVector<QPointF> getData() const;
    QVector<QPointF> getRange(double xMin, double xMax) const;
    void clear();
    int size() const;
    bool isEmpty() const;

    QPointF latestPoint() const;

private:
    mutable QReadWriteLock m_lock;
    QVector<QPointF> m_data;
    int m_maxSize;
};

#endif // DATABUFFER_H
