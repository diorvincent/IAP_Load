#include "databuffer.h"

DataBuffer::DataBuffer(int maxSize, QObject *parent)
    : QObject(parent)
    , m_maxSize(maxSize)
{
}

void DataBuffer::append(double x, double y)
{
    append(QPointF(x, y));
}

void DataBuffer::append(const QPointF &point)
{
    QWriteLocker locker(&m_lock);
    m_data.append(point);
    while (m_data.size() > m_maxSize) {
        m_data.removeFirst();
    }
    locker.unlock();
}

QVector<QPointF> DataBuffer::getData() const
{
    QReadLocker locker(&m_lock);
    return m_data;
}

QVector<QPointF> DataBuffer::getRange(double xMin, double xMax) const
{
    QReadLocker locker(&m_lock);
    QVector<QPointF> result;
    for (const auto &pt : m_data) {
        if (pt.x() >= xMin && pt.x() <= xMax)
            result.append(pt);
    }
    return result;
}

void DataBuffer::clear()
{
    QWriteLocker locker(&m_lock);
    m_data.clear();
}

int DataBuffer::size() const
{
    QReadLocker locker(&m_lock);
    return m_data.size();
}

bool DataBuffer::isEmpty() const
{
    QReadLocker locker(&m_lock);
    return m_data.isEmpty();
}

QPointF DataBuffer::latestPoint() const
{
    QReadLocker locker(&m_lock);
    if (m_data.isEmpty()) return QPointF();
    return m_data.last();
}
