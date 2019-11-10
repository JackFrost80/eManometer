#include "timer.h"
#include <Arduino.h>

timeout::timeout()
	: ts(millis()), dur(0), expired(false)
{

}

timeout::timeout(int time_ms)
{
	ts = millis();
	dur = time_ms;
	expired = false;
}

void set_timer(timeout& t, int time_ms)
{
	t.ts = millis();
	t.dur = time_ms;
	t.expired = false;
}

bool timer_expired(timeout& t)
{
	unsigned long now = millis();
	unsigned long elapsed = now - t.ts;

	if (!t.expired && elapsed >= t.dur) {
		t.expired = true;
		return true;
	}

	return false;
}

unsigned timer_mgr::ms_timer_ids = 0;

timer_mgr::timer_mgr()
{

}

timer timer_mgr::create_timer(int period, bool one_shot, timer_cb callback)
{
	timer_handle t = { ++ms_timer_ids, callback, period, one_shot, millis() };
	m_timers.push_back(t);

	timer ret;
	ret.id = ms_timer_ids;
	return ret;
}

void timer_mgr::check_timers()
{
	// check if any timer has expired
	auto it = m_timers.begin();
	while (it != m_timers.end()) {
		// store iterator to the next element as we might delete this iterator
		auto next(it);
		++next;

		auto& timer = *it;

		unsigned long now = millis();
		unsigned long elapsed = now - timer.last;

		if (elapsed >= timer.period) {
			if (timer.cb)
				timer.cb();

			if (timer.one_shot)
				m_timers.erase(it);
			else
				timer.last = now;
		}

		it = next;
	}
}

void timer_mgr::delete_timer(const timer& timer)
{
	auto it = m_timers.begin();

  for (; it != m_timers.end(); ++it) {
    if (it->id == timer.id) {
      m_timers.erase(it);
      return;
    }
  }
}

