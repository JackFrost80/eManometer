#ifndef EMANO_TIMER_H
#define EMANO_TIMER_H

#include <functional>
#include <list>

class timer_mgr;

struct timer {
  unsigned int id;

  friend class timer_mgr;
};

class timer_mgr {
  public:
    typedef std::function<void()> timer_cb;

    timer_mgr();

    timer create_timer(int period, bool one_shot, timer_cb callback);
    void check_timers();
    void delete_timer(const timer& timer);

  private:
    struct timer_handle {
      unsigned id;
      timer_cb cb;
      int period;
      bool one_shot;
      unsigned long last;
    };

  static unsigned ms_timer_ids;

  std::list<timer_handle> m_timers;

};

#endif