#!/usr/bin/env python3
"""
EPS limit finder for Hyundai CANFD (EV6).

Writes sweep parameters to /tmp/eps_sweep.json, which the carcontroller
reads every 1s. Monitor live EPS state and adjust limits interactively.

Prerequisites:
  - controlsd jerk limits disabled (drive_helpers.py)
  - panda safety limits widened (hyundai_canfd.h)
  - carcontroller reads /tmp/eps_sweep.json

Usage:
  python tools/lateral_maneuvers/eps_sweep.py

Keys:
  u/U  - increase/decrease delta_up (wind-up rate)
  d/D  - increase/decrease delta_down (wind-down rate)
  m/M  - increase/decrease steer_max (max torque)
  b    - binary search mode (auto-step after each maneuver)
  r    - reset to defaults
  q    - quit
"""

import json
import sys
import time
import tty
import termios
import select
from dataclasses import dataclass, asdict

import cereal.messaging as messaging

SWEEP_FILE = "/tmp/eps_sweep.json"

# stock CANFD values
DEFAULTS = {"delta_up": 10, "delta_down": 15, "steer_max": 270}

@dataclass
class SweepState:
  delta_up: int = 10
  delta_down: int = 15
  steer_max: int = 270

  def write(self):
    with open(SWEEP_FILE, "w") as f:
      json.dump(asdict(self), f)

  def describe(self):
    t_up = self.steer_max / max(self.delta_up, 1) * 0.01
    t_down = self.steer_max / max(self.delta_down, 1) * 0.01
    return (f"delta_up={self.delta_up:3d} ({t_up:.3f}s to peak)  "
            f"delta_down={self.delta_down:3d} ({t_down:.3f}s to zero)  "
            f"steer_max={self.steer_max:3d}")


def get_key_nonblocking():
  """Read a single keypress without blocking."""
  if select.select([sys.stdin], [], [], 0)[0]:
    return sys.stdin.read(1)
  return None


def main():
  sm = messaging.SubMaster(["carState", "carOutput"])
  state = SweepState()
  state.write()

  fault_count = 0
  max_torque_seen = 0
  max_eps_torque_seen = 0
  last_fault_params = None

  # terminal raw mode for key input
  old_settings = termios.tcgetattr(sys.stdin)
  try:
    tty.setcbreak(sys.stdin.fileno())

    print("\033[2J\033[H")  # clear screen
    print("=== EPS LIMIT SWEEP ===")
    print("Keys: u/U=delta_up  d/D=delta_down  m/M=steer_max  r=reset  q=quit")
    print()

    step = 5  # adjustment step size

    while True:
      sm.update(100)  # 100ms timeout

      # read carstate
      cs = sm["carState"]
      co = sm["carOutput"]

      v_ego_kph = cs.vEgo * 3.6
      steer_torque = cs.steeringTorque  # driver column torque
      eps_torque = cs.steeringTorqueEps  # EPS output torque
      fault_temp = cs.steerFaultTemporary
      fault_perm = cs.steerFaultPermanent
      angle = cs.steeringAngleDeg

      cmd_torque = co.actuatorsOutput.torqueOutputCan if sm.updated["carOutput"] else 0

      max_torque_seen = max(max_torque_seen, abs(cmd_torque))
      max_eps_torque_seen = max(max_eps_torque_seen, abs(eps_torque))

      if fault_temp or fault_perm:
        fault_count += 1
        last_fault_params = state.describe()

      # handle key input
      key = get_key_nonblocking()
      if key:
        if key == "q":
          break
        elif key == "u":
          state.delta_up = min(state.delta_up + step, 512)
        elif key == "U":
          state.delta_up = max(state.delta_up - step, 1)
        elif key == "d":
          state.delta_down = min(state.delta_down + step, 512)
        elif key == "D":
          state.delta_down = max(state.delta_down - step, 1)
        elif key == "m":
          state.steer_max = min(state.steer_max + 10, 512)
        elif key == "M":
          state.steer_max = max(state.steer_max - 10, 10)
        elif key == "r":
          state = SweepState(**DEFAULTS)
          fault_count = 0
          max_torque_seen = 0
          max_eps_torque_seen = 0
          last_fault_params = None
        state.write()

      # display
      fault_str = "\033[91mFAULT\033[0m" if (fault_temp or fault_perm) else "\033[92m OK  \033[0m"
      print(f"\033[5;1H"  # move cursor to row 5
            f"Speed: {v_ego_kph:5.1f} km/h   Angle: {angle:7.1f}°   EPS: {fault_str}\n"
            f"Cmd torque: {cmd_torque:4.0f}   EPS torque: {eps_torque:7.1f}   Driver: {steer_torque:7.1f}\n"
            f"Peak cmd: {max_torque_seen:4.0f}   Peak EPS: {max_eps_torque_seen:7.1f}   Faults: {fault_count}\n"
            f"\n"
            f"Limits: {state.describe()}\n"
            f"Last fault at: {last_fault_params or 'none'}\033[K\n"
            f"\033[K")

      time.sleep(0.05)

  finally:
    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
    # restore defaults on exit
    SweepState(**DEFAULTS).write()
    print("\nRestored defaults.")


if __name__ == "__main__":
  main()
