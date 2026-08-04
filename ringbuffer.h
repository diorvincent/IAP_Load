#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <QVector>
#include <QMutex>
#include <QMutexLocker>

template<typename T>
class RingBuffer
{
public:
    explicit RingBuffer(int capacity = 1000)
        : m_capacity(capacity)
        , m_buffer(capacity)
        , m_size(0)
        , m_head(0)
        , m_tail(0)
    {
    }

    void append(const T &value)
    {
        QMutexLocker locker(&m_mutex);
        m_buffer[m_tail] = value;
        m_tail = (m_tail + 1) % m_capacity;
        if (m_size < m_capacity) {
            m_size++;
        } else {
            m_head = (m_head + 1) % m_capacity;
        }
    }

    QVector<T> toVector() const
    {
        QMutexLocker locker(&m_mutex);
        QVector<T> result;
        result.reserve(m_size);
        for (int i = 0; i < m_size; ++i) {
            result.append(m_buffer[(m_head + i) % m_capacity]);
        }
        return result;
    }

    void clear()
    {
        QMutexLocker locker(&m_mutex);
        m_size = 0;
        m_head = 0;
        m_tail = 0;
    }

    int size() const
    {
        QMutexLocker locker(&m_mutex);
        return m_size;
    }

    bool isEmpty() const
    {
        QMutexLocker locker(&m_mutex);
        return m_size == 0;
    }

    T latest() const
    {
        QMutexLocker locker(&m_mutex);
        if (m_size == 0) return T();
        return m_buffer[(m_tail - 1 + m_capacity) % m_capacity];
    }

private:
    mutable QMutex m_mutex;
    int m_capacity;
    QVector<T> m_buffer;
    int m_size;
    int m_head;
    int m_tail;
};

#endif // RINGBUFFER_H
