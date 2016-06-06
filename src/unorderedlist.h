#ifndef _UNORDERED_LIST_H
#define _UNORDERED_LIST_H

#include "logging.h"

template<typename T>
class UnorderedList
{
public:
    UnorderedList()
        : m_capacity(8), m_count(0), m_data(0)
    {
        m_data = new T[m_capacity];
    }

    UnorderedList(int initialCapacity)
        : m_capacity(initialCapacity), m_count(0), m_data(0)
    {
        m_data = new T[m_capacity];
    }

    ~UnorderedList()
    {
        if(m_data != nullptr)
        {
            delete[] m_data;
        }
    }

    void insert(T val)
    {
        if(m_count >= m_capacity)
        {
            int newCapacity = m_capacity*2;
            T* newData = new T[newCapacity];
            memcpy(newData, m_data, m_count*sizeof(T));

            delete[] m_data;
            m_data = newData;
            m_capacity = newCapacity;
        }

        m_data[m_count] = val;
        ++m_count;
    }

    void removeAt(int index)
    {
#ifndef NDEBUG
        if((index < 0) || (index >= m_count))
        {
            logWarn("Attempt to remove invalid index %i, valid range is [0-%i]\n", index, m_count-1);
        }
#endif
        if(m_count > 1)
        {
            m_data[index] = m_data[m_count-1];
        }
        --m_count;
    }

    // TODO: Regarding calling of destructors, what happens if I have obj A and obj B and I set B = A,
    //       does it call B's destructor before overwriting constructing the new value?
    void clear()
    {
        m_count = 0;
    }

    // NOTE: This does the same thing as clear but it runs delete on each element first
    //       As a result this should only be called when the contents of the list is type*
    //
    //       Also note that this runs "delete item", not "delete[] item", so we get undefined
    //       behaviour if your pointer stores an array
    void pointerClear()
    {
        for(int i=0; i<m_count; ++i)
        {
            delete m_data[i];
        }
        m_count = 0;
    }

    T operator [](int index)
    {
#ifndef NDEBUG
        if((index < 0) || (index >= m_count))
        {
            logWarn("Invalid index %i requested, valid range is [0-%i]\n", index, m_count-1);
        }
#endif
        return m_data[index];
    }

    int size()
    {
        return m_count;
    }

private:
    int m_capacity;
    int m_count;
    T* m_data;
};

#endif
