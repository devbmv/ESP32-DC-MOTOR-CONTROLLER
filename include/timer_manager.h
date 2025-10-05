#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>

// ============================================================
// 🕒 TimerTask - A single scheduled task
// ------------------------------------------------------------
// Holds a function callback, interval, last run timestamp,
// and whether the task repeats (interval) or runs once (timeout).
// ============================================================
struct TimerTask
{
  std::function<void()> callback;  // Function to execute
  unsigned long interval;          // Interval in ms
  unsigned long lastRun;           // Last time the function ran
  bool repeat;                     // True = setInterval, False = setTimeout
  bool active;                     // Whether the task is currently active

  TimerTask(std::function<void()> cb, unsigned long intv, bool rpt)
      : callback(cb), interval(intv), lastRun(millis()), repeat(rpt), active(true) {}
};

// ============================================================
// 🧩 SimpleTimerManager
// ------------------------------------------------------------
// A lightweight cooperative timer manager.
// - Non-blocking (based on millis())
// - Works like JavaScript’s setTimeout / setInterval
// - Ideal for ESP32 tasks and microcontroller loops
// ============================================================
class SimpleTimerManager
{
public:
  // Schedule a one-time task to run after `delayMs` milliseconds
  void setTimeout(std::function<void()> cb, unsigned long delayMs)
  {
    tasks.emplace_back(cb, delayMs, false);
  }

  // Schedule a repeating task every `intervalMs` milliseconds
  void setInterval(std::function<void()> cb, unsigned long intervalMs)
  {
    tasks.emplace_back(cb, intervalMs, true);
  }

  // Must be called frequently (e.g., inside loop() or main task)
  void update()
  {
    unsigned long now = millis();
    for (auto &task : tasks)
    {
      if (!task.active)
        continue;

      if ((now - task.lastRun) >= task.interval)
      {
        task.callback();
        task.lastRun += task.interval;

        if (!task.repeat)
          task.active = false;
      }
    }
  }

private:
  std::vector<TimerTask> tasks;  // List of all scheduled tasks
};
