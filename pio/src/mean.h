#ifndef EMANO_MEAN_H
#define EMANO_MEAN_H

#include <vector>

template<typename T>
class mean {
  public:
    mean()
    {
    }

    void init(int depth, T def)
    {
      m_depth = depth;
      m_pos = 0;
      std::vector<T> n(depth, def);
      vals.swap(n);
    }

    void add(T val)
    {
      vals[m_pos++] = val;
      m_pos %= m_depth;
    }

    T get()
    {
      T sum = 0;
      for (int val: vals) {
        sum += val;
      }

      return sum / m_depth;
    }

  private:
    int m_pos{0};
    int m_depth{0};
    std::vector<T> vals;
};

#endif
