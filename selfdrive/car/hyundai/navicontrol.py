# created by atom
import math
import numpy as np

from selfdrive.config import Conversions as CV
from selfdrive.car.hyundai.values import Buttons
from common.numpy_fast import clip, interp
from cereal import log
import cereal.messaging as messaging
from common.params import Params

import common.log as trace1
import common.MoveAvg as mvAvg

LaneChangeState = log.LateralPlan.LaneChangeState

class NaviControl():
  def __init__(self, p=None):
    self.p = p
    
    self.sm = messaging.SubMaster(['liveNaviData', 'lateralPlan', 'radarState']) 

    self.btn_cnt = 0
    self.seq_command = 0
    self.target_speed = 0
    self.set_point = 0
    self.wait_timer2 = 0
    self.wait_timer3 = 0

    self.moveAvg = mvAvg.MoveAvg()

    self.gasPressed_old = 0

    self.map_spdlimit_offset = int(Params().get("OpkrSpeedLimitOffset", encoding="utf8"))
    self.safetycam_decel_dist_gain = int(Params().get("SafetyCamDecelDistGain", encoding="utf8"))

    self.map_speed_block = False
    self.map_speed_dist = 0
    self.map_speed = 0
    self.map_speed_control_start = False
    self.onSpeedControl = False
    self.map_speed_dist_prev = 0
    self.ctrl_speed = 0

  def update_lateralPlan(self):
    self.sm.update(0)
    path_plan = self.sm['lateralPlan']
    return path_plan

  def button_status(self, CS):
    if not CS.cruise_active or CS.cruise_buttons != Buttons.NONE: 
      self.wait_timer2 = 50 
    elif self.wait_timer2: 
      self.wait_timer2 -= 1
    else:
      return 1
    return 0

  # buttn acc,dec control
  def switch(self, seq_cmd):
      self.case_name = "case_" + str(seq_cmd)
      self.case_func = getattr( self, self.case_name, lambda:"default")
      return self.case_func()

  def reset_btn(self):
      if self.seq_command != 3:
        self.seq_command = 0

  def case_default(self):
      self.seq_command = 0
      return None

  def case_0(self):
      self.btn_cnt = 0
      self.target_speed = self.set_point
      delta_speed = round(self.target_speed - self.VSetDis)
      if delta_speed > 0:
        self.seq_command = 1
      elif delta_speed < 0:
        self.seq_command = 2
      return None

  def case_1(self):  # acc
      btn_signal = Buttons.RES_ACCEL
      self.btn_cnt += 1
      if self.target_speed == self.VSetDis:
        self.btn_cnt = 0
        self.seq_command = 3            
      elif self.btn_cnt > 10:
        self.btn_cnt = 0
        self.seq_command = 3
      return btn_signal

  def case_2(self):  # dec
      btn_signal = Buttons.SET_DECEL
      self.btn_cnt += 1
      if self.target_speed == self.VSetDis:
        self.btn_cnt = 0
        self.seq_command = 3            
      elif self.btn_cnt > 10:
        self.btn_cnt = 0
        self.seq_command = 3
      return btn_signal

  def case_3(self):  # None
      btn_signal = None  # Buttons.NONE
      
      self.btn_cnt += 1
      #if self.btn_cnt == 1:
      #  btn_signal = Buttons.NONE
      if self.btn_cnt > 5: 
        self.seq_command = 0
      return btn_signal

  def ascc_button_control(self, CS, set_speed):
    self.set_point = max(20 if CS.is_set_speed_in_mph else 30, set_speed)
    self.curr_speed = CS.out.vEgo * CV.MS_TO_KPH
    self.VSetDis = round(CS.VSetDis)
    btn_signal = self.switch(self.seq_command)

    return btn_signal

  def get_navi_speed(self, sm, CS, cruiseState_speed):
    cruise_set_speed_kph = cruiseState_speed
    v_ego_kph = CS.out.vEgo * CV.MS_TO_KPH
    self.liveNaviData = sm['liveNaviData']
    # speedLimit = self.liveNaviData.speedLimit
    # speedLimitDistance = self.liveNaviData.speedLimitDistance  #speedLimitDistance
    # safetySign = self.liveNaviData.safetySign
    #mapValid = self.liveNaviData.mapValid
    #trafficType = self.liveNaviData.trafficType
    
    #if not mapValid or trafficType == 0:
    #  return  cruise_set_speed_kph
    if CS.map_enabled and self.liveNaviData.speedLimit:
      self.map_speed_dist = self.liveNaviData.speedLimitDistance
      if self.map_speed_dist_prev != self.map_speed_dist:
        self.map_speed_dist_prev = self.map_speed_dist
        self.map_speed = self.liveNaviData.speedLimit
        if self.map_speed > 29:
          if self.map_speed_dist > 1250:
            self.map_speed_block = True
        else:
          self.map_speed_block = False
      if self.map_speed > 29:
        cam_distance_calc = 0
        cam_distance_calc = interp(v_ego_kph, [30, 110], [2.8, 4.0])
        consider_speed = interp((v_ego_kph - self.map_speed), [0, 50], [1, 2.25])
        min_control_dist = interp(self.map_speed, [30, 110], [40, 250])
        final_cam_decel_start_dist = cam_distance_calc*consider_speed*v_ego_kph * (1 + self.safetycam_decel_dist_gain*0.01)
        if self.map_speed_dist < final_cam_decel_start_dist:
          spdTarget = self.map_speed
          self.map_speed_control_start = True
          self.onSpeedControl = True
        elif self.map_speed_dist >= final_cam_decel_start_dist and self.map_speed_block:
          self.map_speed_control_start = True
          spdTarget = self.map_speed
          self.onSpeedControl = True
        elif self.map_speed_dist < min_control_dist:
          self.map_speed_control_start = True
          spdTarget = self.map_speed
          self.onSpeedControl = True
        elif self.map_speed_control_start:
          spdTarget = self.map_speed
          self.onSpeedControl = True
        else:
          self.map_speed_control_start = False
          self.onSpeedControl = False
          return cruise_set_speed_kph
        cruise_set_speed_kph = spdTarget + round(spdTarget*0.01*self.map_spdlimit_offset)
      else:
        self.map_speed_control_start = False
        spdTarget = cruise_set_speed_kph
        self.onSpeedControl = False
        self.map_speed = 0
        self.map_speed_dist = 0
        self.map_speed_dist_prev = 0
        return cruise_set_speed_kph
    elif CS.safety_dist:
      self.map_speed_dist = CS.safety_dist
      if self.map_speed_dist_prev != self.map_speed_dist:
        self.map_speed_dist_prev = self.map_speed_dist
        self.map_speed = CS.safety_sign
        if self.map_speed > 29:
          if CS.safety_block_remain_dist < 255:
            self.map_speed_block = True
        else:
          self.map_speed_block = False
      if self.map_speed > 29:
        cam_distance_calc = 0
        cam_distance_calc = interp(v_ego_kph, [30, 110], [2.8, 4.0])
        consider_speed = interp((v_ego_kph - self.map_speed), [0, 50], [1, 2.25])
        min_control_dist = interp(self.map_speed, [30, 110], [40, 250])
        final_cam_decel_start_dist = cam_distance_calc*consider_speed*v_ego_kph * (1 + self.safetycam_decel_dist_gain*0.01)
        if self.map_speed_dist < final_cam_decel_start_dist:
          spdTarget = self.map_speed
          self.map_speed_control_start = True
          self.onSpeedControl = True
        elif self.map_speed_dist >= final_cam_decel_start_dist and self.map_speed_block:
          self.map_speed_control_start = True
          spdTarget = self.map_speed
          self.onSpeedControl = True
        elif self.map_speed_dist < min_control_dist:
          self.map_speed_control_start = True
          spdTarget = self.map_speed
          self.onSpeedControl = True
        elif self.map_speed_control_start:
          spdTarget = self.map_speed
          self.onSpeedControl = True
        else:
          self.map_speed_control_start = False
          self.onSpeedControl = False
          return cruise_set_speed_kph
        cruise_set_speed_kph = spdTarget + round(spdTarget*0.01*self.map_spdlimit_offset)
      else:
        self.map_speed_control_start = False
        spdTarget = cruise_set_speed_kph
        self.onSpeedControl = False
        self.map_speed = 0
        self.map_speed_dist = 0
        self.map_speed_dist_prev = 0
        return cruise_set_speed_kph
    else:
      self.onSpeedControl = False

    # elif speedLimitDistance >= 50:
    #   if speedLimit <= 60:
    #     spdTarget = interp(speedLimitDistance, [50, 600], [ speedLimit, speedLimit + 50 ])
    #   else:
    #     spdTarget = interp(speedLimitDistance, [150, 900], [ speedLimit, speedLimit + 30 ])
    # else:
    #   spdTarget = speedLimit

    # if v_ego_kph < speedLimit:
    #   v_ego_kph = speedLimit

    # print('cruise_set_speed_kph={}'.format(cruise_set_speed_kph))

    return cruise_set_speed_kph

  def variable_cruise(self, CS, var_cruise_speed):
    cruiseState_speed = round(CS.out.cruiseState.speed * CV.MS_TO_KPH)
    self.lead_0 = self.sm['radarState'].leadOne
    self.lead_1 = self.sm['radarState'].leadTwo

    if self.lead_0.status:
      dRel = int(self.lead_0.dRel)
      vRel = int(self.lead_0.vRel * CV.MS_TO_KPH)
      if vRel >= -5:
        var_speed = min(var_cruise_speed + max(0, dRel*0.2+vRel), cruiseState_speed)
      else:
        var_speed = min(var_cruise_speed, cruiseState_speed)
    else:
      var_speed = cruiseState_speed

    # print('status={}  dRel={}  vRel={}  var_speed={}  cruiseState_speed={}'.format(
    #   self.lead_0.status, int(self.lead_0.dRel), int(self.lead_0.vRel * CV.MS_TO_KPH), var_speed, cruiseState_speed))

    return round(var_speed)

  def auto_speed_control(self, CS, ctrl_speed, path_plan):
    modelSpeed = path_plan.modelSpeed
    min_control_speed = 20 if CS.is_set_speed_in_mph else 30
    if CS.driverAcc_time:
      return CS.clu_Vanz + 3
    # elif self.gasPressed_old:
    #   clu_Vanz = CS.clu_Vanz
    #   ctrl_speed = max(min_control_speed, ctrl_speed, clu_Vanz)
    #   CS.set_cruise_speed(ctrl_speed)
    elif CS.CP.resSpeed > 30:
      ctrl_speed = max(min_control_speed, CS.CP.resSpeed)
      return ctrl_speed
    elif CS.cruise_set_mode in [1,2,4]:
      if CS.CP.vFuture >= min_control_speed:
        ctrl_speed = self.variable_cruise(CS, CS.CP.vFuture)
      elif 1 < CS.CP.vFuture < min_control_speed:
        ctrl_speed = min_control_speed

    if CS.cruise_set_mode in [1,3,4] and CS.out.vEgo * CV.MS_TO_KPH > 40 and modelSpeed < 90 and \
     path_plan.laneChangeState == LaneChangeState.off and not (CS.out.leftBlinker or CS.out.rightBlinker):
      ctrl_speed = min(ctrl_speed, interp(modelSpeed, [30, 40, 50, 60, 70, 80, 90], [40, 45, 50, 55, 65, 75, 85])) # curve speed ratio

    # self.gasPressed_old = CS.gasPressed
    return ctrl_speed

  def update(self, CS, path_plan):
    btn_signal = None
    if not self.button_status(CS):
      pass
    elif CS.cruise_active:
      cruiseState_speed = round(CS.out.cruiseState.speed * CV.MS_TO_KPH)
      kph_set_vEgo = self.get_navi_speed(self.sm, CS, cruiseState_speed) # camspeed
      self.ctrl_speed = min(cruiseState_speed, kph_set_vEgo)

      print('self.ctrl_speed={}  kph_set_vEgo={}  cruiseState_speed={}'.format(self.ctrl_speed, kph_set_vEgo, cruiseState_speed))

      # if CS.cruise_set_mode != 5:
      #   self.ctrl_speed = self.auto_speed_control(CS, self.ctrl_speed, path_plan) # lead, curve speed

      btn_signal = self.ascc_button_control(CS, self.ctrl_speed)

    return btn_signal