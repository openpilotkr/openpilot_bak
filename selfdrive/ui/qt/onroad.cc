#include "selfdrive/ui/qt/onroad.h"

#include <cmath>

#include <QDebug>
#include <QFileInfo>
#include <QDateTime>

#include "selfdrive/common/timing.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/api.h"
#include "selfdrive/ui/qt/widgets/input.h"

#ifdef ENABLE_MAPS
#include "selfdrive/ui/qt/maps/map.h"
#endif

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(bdr_s);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  QStackedLayout *road_view_layout = new QStackedLayout;
  road_view_layout->setStackingMode(QStackedLayout::StackAll);
  nvg = new NvgWindow(VISION_STREAM_RGB_BACK, this);
  road_view_layout->addWidget(nvg);
  hud = new OnroadHud(this);
  road_view_layout->addWidget(hud);

  QWidget * split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);
  split->addLayout(road_view_layout);

  stacked_layout->addWidget(split_wrapper);

  alerts = new OnroadAlerts(this);
  alerts->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  stacked_layout->addWidget(alerts);

  // setup stacking order
  alerts->raise();

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(this, &OnroadWindow::updateStateSignal, this, &OnroadWindow::updateState);
  QObject::connect(this, &OnroadWindow::offroadTransitionSignal, this, &OnroadWindow::offroadTransition);
}

void OnroadWindow::updateState(const UIState &s) {
  QColor bgColor = bg_colors[s.status];
  Alert alert = Alert::get(*(s.sm), s.scene.started_frame);
  if (s.sm->updated("controlsState") || !alert.equal({})) {
    if (alert.type == "controlsUnresponsive") {
      bgColor = bg_colors[STATUS_ALERT];
    }
    if (!QUIState::ui_state.is_OpenpilotViewEnabled) {
      // opkr
      if (QFileInfo::exists("/data/log/error.txt") && s.scene.show_error && !s.scene.tmux_error_check) {
        QFileInfo fileInfo;
        fileInfo.setFile("/data/log/error.txt");
        QDateTime modifiedtime = fileInfo.lastModified();
        QString modified_time = modifiedtime.toString("yyyy-MM-dd hh:mm:ss ");
        const std::string txt = util::read_file("/data/log/error.txt");
        if (RichTextDialog::alert(modified_time + QString::fromStdString(txt), this)) {
          QUIState::ui_state.scene.tmux_error_check = true;
        }
      }
	  alerts->updateAlert(alert, bgColor);
    }
  }

  hud->updateState(s);

  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    update();
  }
}

void OnroadWindow::mousePressEvent(QMouseEvent* e) {
  // propagation event to parent(HomeWindow)
  QWidget::mousePressEvent(e);

  if ((map_overlay_btn.contains(e->pos()) || map_btn.contains(e->pos()) || map_return_btn.contains(e->pos()) || 
    rec_btn.contains(e->pos()) || laneless_btn.contains(e->pos()) || monitoring_btn.contains(e->pos()) || speedlimit_btn.contains(e->pos()) ||
    stockui_btn.contains(e->pos()) || tuneui_btn.contains(e->pos()) || mapbox_btn.contains(e->pos()) || QUIState::ui_state.scene.map_on_top || 
    QUIState::ui_state.scene.live_tune_panel_enable)) {return;}
  if (map != nullptr) {
    bool sidebarVisible = geometry().x() > 0;
    map->setVisible(!sidebarVisible && !map->isVisible());
    if (map->isVisible()) {
      QUIState::ui_state.scene.mapbox_running = true;
    } else {
      QUIState::ui_state.scene.mapbox_running = false;
    }
  }
}

void OnroadWindow::offroadTransition(bool offroad) {
#ifdef ENABLE_MAPS
  if (!offroad) {
    QString token = QString::fromStdString(Params().get("dp_mapbox_token_sk"));
    if (map == nullptr && !token.isEmpty() && Params().getBool("MapboxEnabled")) {
      QMapboxGLSettings settings;

      // // Valid for 4 weeks since we can't swap tokens on the fly
      // QString token = MAPBOX_TOKEN.isEmpty() ? CommaApi::create_jwt({}, 4 * 7 * 24 * 3600) : MAPBOX_TOKEN;

      if (!Hardware::PC()) {
        settings.setCacheDatabasePath("/data/mbgl-cache.db");
      }
      settings.setApiBaseUrl(MAPS_HOST);
      settings.setCacheDatabaseMaximumSize(20 * 1024 * 1024);
      settings.setAccessToken(token.trimmed());

      MapWindow * m = new MapWindow(settings);
      m->setFixedWidth(topWidget(this)->width() / 2);
      QObject::connect(this, &OnroadWindow::offroadTransitionSignal, m, &MapWindow::offroadTransition);
      split->addWidget(m, 0, Qt::AlignRight);
      map = m;
    }
  }
#endif

  alerts->updateAlert({}, bg);

  // update stream type
  bool wide_cam = Hardware::TICI() && Params().getBool("EnableWideCamera");
  nvg->setStreamType(wide_cam ? VISION_STREAM_RGB_WIDE : VISION_STREAM_RGB_BACK);
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(bg.red(), bg.green(), bg.blue(), 255));
}

// ***** onroad widgets *****

// OnroadAlerts
void OnroadAlerts::updateAlert(const Alert &a, const QColor &color) {
  if (!alert.equal(a) || color != bg) {
    alert = a;
    bg = color;
    update();
  }
}

void OnroadAlerts::paintEvent(QPaintEvent *event) {
  if (alert.size == cereal::ControlsState::AlertSize::NONE) {
    return;
  }
  static std::map<cereal::ControlsState::AlertSize, const int> alert_sizes = {
    {cereal::ControlsState::AlertSize::SMALL, 271},
    {cereal::ControlsState::AlertSize::MID, 420},
    {cereal::ControlsState::AlertSize::FULL, height()},
  };
  int h = alert_sizes[alert.size];
  QRect r = QRect(0, height() - h, width(), h);

  QPainter p(this);

  // draw background + gradient
  p.setPen(Qt::NoPen);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  p.setBrush(QBrush(bg));
  p.drawRect(r);

  QLinearGradient g(0, r.y(), 0, r.bottom());
  g.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.05));
  g.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0.35));

  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.setBrush(QBrush(g));
  p.fillRect(r, g);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // text
  const QPoint c = r.center();
  p.setPen(QColor(0xff, 0xff, 0xff));
  //p.setRenderHint(QPainter::TextAntialiasing);
  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    configFont(p, "Open Sans", 74, "SemiBold");
    p.drawText(r, Qt::AlignCenter, alert.text1);
  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    configFont(p, "Open Sans", 88, "Bold");
    p.drawText(QRect(0, c.y() - 125, width(), 150), Qt::AlignHCenter | Qt::AlignTop, alert.text1);
    configFont(p, "Open Sans", 66, "Regular");
    p.drawText(QRect(0, c.y() + 21, width(), 90), Qt::AlignHCenter, alert.text2);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    configFont(p, "Open Sans", l ? 132 : 177, "Bold");
    p.drawText(QRect(0, r.y() + (l ? 240 : 270), width(), 600), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);
    configFont(p, "Open Sans", 88, "Regular");
    p.drawText(QRect(0, r.height() - (l ? 361 : 420), width(), 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}

// OnroadHud
OnroadHud::OnroadHud(QWidget *parent) : QWidget(parent) {
  engage_img = QPixmap("../assets/img_chffr_wheel.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  dm_img = QPixmap("../assets/img_driver_face.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  connect(this, &OnroadHud::valueChanged, [=] { update(); });
}

void OnroadHud::updateState(const UIState &s) {
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);
  const auto cs = sm["controlsState"].getControlsState();

  float maxspeed = cs.getVCruise();
  float cruisespeed = s.scene.vSetDis;
  bool over_sl = false;
  bool comma_ui = s.scene.comma_stock_ui;
  if (s.scene.limitSCOffsetOption) {
    over_sl = s.scene.limitSpeedCamera > 19 && ((s.scene.limitSpeedCamera+s.scene.speed_lim_off)+1 < s.scene.car_state.getVEgo() * (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH));
  } else {
    over_sl = s.scene.limitSpeedCamera > 19 && ((s.scene.limitSpeedCamera+round(s.scene.limitSpeedCamera*0.01*s.scene.speed_lim_off))+1 < s.scene.car_state.getVEgo() * (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH));
  }

  bool cruise_set = maxspeed > 0 && (int)maxspeed != SET_SPEED_NA;
  // if (cruise_set && !s.scene.is_metric) {
  //   maxspeed *= KM_TO_MILE;
  // }
  QString maxspeed_str = QString::number(std::nearbyint(maxspeed));
  QString cruisespeed_str = QString::number(std::nearbyint(cruisespeed));
  float cur_speed = std::max(0.0, sm["carState"].getCarState().getVEgo() * (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH));

  auto lead_one = sm["radarState"].getRadarState().getLeadOne();
  float drel = lead_one.getDRel();
  float vrel = lead_one.getVRel();
  bool leadstat = lead_one.getStatus();

  setProperty("is_cruise_set", cruise_set);
  setProperty("speed", QString::number(std::nearbyint(cur_speed)));
  setProperty("maxSpeed", maxspeed_str);
  setProperty("cruiseSpeed", cruisespeed_str);
  setProperty("speedUnit", s.scene.is_metric ? "km/h" : "mph");
  setProperty("hideDM", cs.getAlertSize() != cereal::ControlsState::AlertSize::NONE);
  setProperty("status", s.status);
  setProperty("is_over_sl", over_sl);
  setProperty("comma_stock_ui", comma_ui);
  setProperty("lead_stat", leadstat);
  setProperty("dist_rel", drel);
  setProperty("vel_rel", vrel);
  setProperty("ang_str", s.scene.angleSteers);
  setProperty("record_stat", s.scene.rec_stat);
  setProperty("lane_stat", s.scene.laneless_mode);
  setProperty("laneless_stat", s.scene.lateralPlan.lanelessModeStatus);
  setProperty("map_stat", s.scene.map_is_running);
  setProperty("mapbox_stat", s.scene.mapbox_running);
  setProperty("dm_mode", s.scene.monitoring_mode);
  setProperty("ss_elapsed", s.scene.lateralPlan.standstillElapsedTime);
  setProperty("standstill", s.scene.standStill);

  // update engageability and DM icons at 2Hz
  if (sm.frame % (UI_FREQ / 2) == 0) {
    setProperty("engageable", cs.getEngageable() || cs.getEnabled());
    setProperty("dmActive", sm["driverMonitoringState"].getDriverMonitoringState().getIsActiveMode());
  }
}

void OnroadHud::paintEvent(QPaintEvent *event) {
  UIState *s = &QUIState::ui_state;
  QPainter p(this);
  //p.setRenderHint(QPainter::Antialiasing);

  // Header gradient
  QLinearGradient bg(0, header_h - (header_h / 2.5), 0, header_h);
  bg.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.45));
  bg.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
  p.fillRect(0, 0, width(), header_h, bg);

  // max speed
  if (!hideDM) {
    QRect rc(bdr_s, rect().bottom() - bdr_s - 202, 184, 202);
    p.setPen(QPen(QColor(0xff, 0xff, 0xff, 100), 10));
    if (is_over_sl) {
      p.setBrush(QColor(218, 111, 37, 150));
    } else if (s->scene.limitSpeedCamera > 19 && !is_over_sl) {
      p.setBrush(QColor(0, 120, 0, 150));
    } else if (s->scene.cruiseAccStatus) {
      p.setBrush(QColor(0, 100, 200, 150));
    } else if (s->scene.controls_state.getEnabled()) {
      p.setBrush(QColor(255, 255, 255, 75));
    } else {
      p.setBrush(QColor(0, 0, 0, 100));
    }
    p.drawRoundedRect(rc, 20, 20);
    p.setPen(Qt::NoPen);
    if (cruiseSpeed >= 20 && s->scene.controls_state.getEnabled()) {
      configFont(p, "Open Sans", 70, "Bold");
      drawText(p, rc.center().x(), rect().bottom() - bdr_s - 127, maxSpeed, 255);
    } else {
      configFont(p, "Open Sans", 70, "SemiBold");
      drawText(p, rc.center().x(), rect().bottom() - bdr_s - 127, "-", is_cruise_set ? 200 : 100);
    }
    if (is_cruise_set) {
      configFont(p, "Open Sans", 90, "Bold");
      drawText(p, rc.center().x(), rect().bottom() - bdr_s - 32, cruiseSpeed, 255);
    } else {
      configFont(p, "Open Sans", 90, "SemiBold");
      drawText(p, rc.center().x(), rect().bottom() - bdr_s - 32, "-", 100);
    }
  }

  // current speed
  configFont(p, "Open Sans", 176, "Bold");
  drawText(p, rect().center().x(), 210, speed);
  configFont(p, "Open Sans", 66, "Regular");
  drawText(p, rect().center().x(), 290, speedUnit, 200);

  // engage-ability icon
  //if (engageable) {
  if (true) {
    drawIcon(p, rect().right() - radius / 2 - bdr_s, radius / 2 + bdr_s,
             engage_img, bg_colors[status], 1.0, true, ang_str);
  }

  // dm icon
  if (true) {
    drawIcon(p, radius / 2 + bdr_s, radius / 2 + bdr_s,
             dm_img, dm_mode ? QColor(10, 120, 20, 70) : QColor(0, 0, 0, 70), dmActive ? 1.0 : 0.2);
  }

  p.setBrush(QColor(0, 0, 0, 0));
  p.setPen(whiteColor(150));
  //p.setRenderHint(QPainter::TextAntialiasing);
  p.setOpacity(0.7);
  int ui_viz_rx = bdr_s + 190;
  int ui_viz_ry = bdr_s + 100;
  int ui_viz_rx_center = s->fb_w/2;
  // debug
  if (s->scene.nDebugUi1 && !comma_stock_ui) {
    configFont(p, "Open Sans", s->scene.mapbox_running?20:25, "Semibold");
    uiText(p, 205, 1030-bdr_s+(s->scene.mapbox_running?15:0), s->scene.alertTextMsg1.c_str());
    uiText(p, 205, 1060-bdr_s+(s->scene.mapbox_running?5:0), s->scene.alertTextMsg2.c_str());
  }
  if (s->scene.nDebugUi2 && !comma_stock_ui) {
    configFont(p, "Open Sans", s->scene.mapbox_running?26:35, "Semibold");
    uiText(p, ui_viz_rx, ui_viz_ry+240, "SR:" + QString::number(s->scene.liveParams.steerRatio, 'f', 2));
    uiText(p, ui_viz_rx, ui_viz_ry+280, "AA:" + QString::number(s->scene.liveParams.angleOffsetAverage, 'f', 2));
    uiText(p, ui_viz_rx, ui_viz_ry+320, "SF:" + QString::number(s->scene.liveParams.stiffnessFactor, 'f', 2));
    uiText(p, ui_viz_rx, ui_viz_ry+360, "AD:" + QString::number(s->scene.steer_actuator_delay, 'f', 2));
    uiText(p, ui_viz_rx, ui_viz_ry+400, "SC:" + QString::number(s->scene.lateralPlan.steerRateCost, 'f', 2));
    uiText(p, ui_viz_rx, ui_viz_ry+440, "OS:" + QString::number(s->scene.output_scale, 'f', 2));
    uiText(p, ui_viz_rx, ui_viz_ry+480, QString::number(s->scene.lateralPlan.lProb, 'f', 2) + "|" + QString::number(s->scene.lateralPlan.rProb, 'f', 2));

    if (s->scene.map_is_running) {
      if (s->scene.liveNaviData.opkrspeedsign) uiText(p, ui_viz_rx, ui_viz_ry+560, "SS:" + QString::number(s->scene.liveNaviData.opkrspeedsign, 'f', 0));
      if (s->scene.liveNaviData.opkrspeedlimit) uiText(p, ui_viz_rx, ui_viz_ry+600, "SL:" + QString::number(s->scene.liveNaviData.opkrspeedlimit, 'f', 0));
      if (s->scene.liveNaviData.opkrspeedlimitdist) uiText(p, ui_viz_rx, ui_viz_ry+640, "DS:" + QString::number(s->scene.liveNaviData.opkrspeedlimitdist, 'f', 0));
      if (s->scene.liveNaviData.opkrturninfo) uiText(p, ui_viz_rx, ui_viz_ry+680, "TI:" + QString::number(s->scene.liveNaviData.opkrturninfo, 'f', 0));
      if (s->scene.liveNaviData.opkrdisttoturn) uiText(p, ui_viz_rx, ui_viz_ry+680, "DT:" + QString::number(s->scene.liveNaviData.opkrdisttoturn, 'f', 0));
    } else if (!s->scene.map_is_running && (*s->sm)["carState"].getCarState().getSafetySign() > 19) {
      uiText(p, ui_viz_rx, ui_viz_ry+560, "SL:" + QString::number((*s->sm)["carState"].getCarState().getSafetySign(), 'f', 0));
      uiText(p, ui_viz_rx, ui_viz_ry+600, "DS:" + QString::number((*s->sm)["carState"].getCarState().getSafetyDist(), 'f', 0));
    }
    uiText(p, ui_viz_rx+(s->scene.mapbox_running ? 150:200), ui_viz_ry+240, "SL:" + QString::number(s->scene.liveMapData.ospeedLimit, 'f', 0));
    uiText(p, ui_viz_rx+(s->scene.mapbox_running ? 150:200), ui_viz_ry+280, "SLA:" + QString::number(s->scene.liveMapData.ospeedLimitAhead, 'f', 0));
    uiText(p, ui_viz_rx+(s->scene.mapbox_running ? 150:200), ui_viz_ry+320, "SLAD:" + QString::number(s->scene.liveMapData.ospeedLimitAheadDistance, 'f', 0));
    uiText(p, ui_viz_rx+(s->scene.mapbox_running ? 150:200), ui_viz_ry+360, "TSL:" + QString::number(s->scene.liveMapData.oturnSpeedLimit, 'f', 0));
    uiText(p, ui_viz_rx+(s->scene.mapbox_running ? 150:200), ui_viz_ry+400, "TSLED:" + QString::number(s->scene.liveMapData.oturnSpeedLimitEndDistance, 'f', 0));
    uiText(p, ui_viz_rx+(s->scene.mapbox_running ? 150:200), ui_viz_ry+440, "TSLS:" + QString::number(s->scene.liveMapData.oturnSpeedLimitSign, 'f', 0));

    if (s->scene.lateralControlMethod == 0) {      
      drawText(p, ui_viz_rx_center, bdr_s+310, "PID");
    } else if (s->scene.lateralControlMethod == 1) {
      drawText(p, ui_viz_rx_center, bdr_s+310, "INDI");
    } else if (s->scene.lateralControlMethod == 2) {
      drawText(p, ui_viz_rx_center, bdr_s+310, "LQR");
    }
  }

  if (!comma_stock_ui) {
    int j_num = 100;
    // opkr debug info(left panel)
    int width_l = 180;
    int sp_xl = rect().left() + bdr_s + width_l / 2 - 10;
    int sp_yl = bdr_s + 260;
    int num_l = 4;
    if (s->scene.longitudinal_control) {num_l = num_l + 1;}
    QRect left_panel(rect().left() + bdr_s, bdr_s + 200, width_l, 104*num_l);  
    p.setOpacity(1.0);
    p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    p.drawRoundedRect(left_panel, 20, 20);
    p.setPen(whiteColor(200));
    //p.setRenderHint(QPainter::TextAntialiasing);
    // lead drel
    if (lead_stat) {
      if (dist_rel < 5) {
        p.setPen(redColor(200));
      } else if (int(dist_rel) < 15) {
        p.setPen(orangeColor(200));
      }
      if (dist_rel < 10) {
        debugText(p, sp_xl, sp_yl, QString::number(dist_rel, 'f', 1), 150, 58);
      } else {
        debugText(p, sp_xl, sp_yl, QString::number(dist_rel, 'f', 0), 150, 58);
      }
    }
    p.setPen(whiteColor(200));
    debugText(p, sp_xl, sp_yl + 35, QString("REL DIST"), 150, 27);
    p.translate(sp_xl + 90, sp_yl + 20);
    p.rotate(-90);
    p.drawText(0, 0, "m");
    p.resetMatrix();
    // lead spd
    sp_yl = sp_yl + j_num;
    if (int(vel_rel) < -5) {
      p.setPen(redColor(200));
    } else if (int(vel_rel) < 0) {
      p.setPen(orangeColor(200));
    }
    if (lead_stat) {
      debugText(p, sp_xl, sp_yl, QString::number(vel_rel * (s->scene.is_metric ? 3.6 : 2.2369363), 'f', 0), 150, 58);
    } else {
      debugText(p, sp_xl, sp_yl, "-", 150, 58);
    }
    p.setPen(whiteColor(200));
    debugText(p, sp_xl, sp_yl + 35, QString("REL SPED"), 150, 27);
    p.translate(sp_xl + 90, sp_yl + 20);
    p.rotate(-90);
    if (s->scene.is_metric) {p.drawText(0, 0, "km/h");} else {p.drawText(0, 0, "mi/h");}
    p.resetMatrix();
    // steer angle
    sp_yl = sp_yl + j_num;
    p.setPen(greenColor(200));
    if ((int(ang_str) < -50) || (int(ang_str) > 50)) {
      p.setPen(redColor(200));
    } else if ((int(ang_str) < -30) || (int(ang_str) > 30)) {
      p.setPen(orangeColor(200));
    }
    debugText(p, sp_xl, sp_yl, QString::number(ang_str, 'f', 0), 150, 58);
    p.setPen(whiteColor(200));
    debugText(p, sp_xl, sp_yl + 35, QString("STER ANG"), 150, 27);
    p.translate(sp_xl + 90, sp_yl + 20);
    p.rotate(-90);
    p.drawText(0, 0, "       °");
    p.resetMatrix();
    // steer ratio
    sp_yl = sp_yl + j_num;
    debugText(p, sp_xl, sp_yl, QString::number(s->scene.steerRatio, 'f', 2), 150, 58);
    debugText(p, sp_xl, sp_yl + 35, QString("SteerRatio"), 150, 27);
    // cruise gap for long
    if (s->scene.longitudinal_control) {
      sp_yl = sp_yl + j_num;
      if (s->scene.controls_state.getEnabled()) {
        if (s->scene.cruise_gap == s->scene.dynamic_tr_mode) {
          debugText(p, sp_xl, sp_yl, "AUT", 150, 58);
        } else {
          debugText(p, sp_xl, sp_yl, QString::number(s->scene.cruise_gap, 'f', 0), 150, 58);
        }
      } else {
        debugText(p, sp_xl, sp_yl, "-", 150, 58);
      }
      debugText(p, sp_xl, sp_yl + 35, QString("CruiseGap"), 150, 27);
      if (s->scene.cruise_gap == s->scene.dynamic_tr_mode) {
        p.translate(sp_xl + 90, sp_yl + 20);
        p.rotate(-90);
        p.drawText(0, 0, QString::number(s->scene.dynamic_tr_value, 'f', 0));
        p.resetMatrix();
      }
    }

    // opkr debug info(right panel)
    int width_r = 180;
    int sp_xr = rect().right() - bdr_s - width_r / 2 - 10;
    int sp_yr = bdr_s + 260;
    int num_r = 1;
    if (s->scene.batt_less) {num_r = num_r + 1;} else {num_r = num_r + 2;}
    if (s->scene.gpsAccuracyUblox != 0.00) {num_r = num_r + 2;}
    QRect right_panel(rect().right() - bdr_s - width_r, bdr_s + 200, width_r, 104*num_r);  
    p.setOpacity(1.0);
    p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    p.drawRoundedRect(right_panel, 20, 20);
    p.setPen(whiteColor(200));
    //p.setRenderHint(QPainter::TextAntialiasing);
    // cpu temp
    if (s->scene.cpuTemp > 85) {
      p.setPen(redColor(200));
    } else if (s->scene.cpuTemp > 75) {
      p.setPen(orangeColor(200));
    }
    debugText(p, sp_xr, sp_yr, QString::number(s->scene.cpuTemp, 'f', 0) + "°C", 150, 58);
    p.setPen(whiteColor(200));
    debugText(p, sp_xr, sp_yr + 35, QString("CPU TEMP"), 150, 27);
    p.translate(sp_xr + 90, sp_yr + 20);
    p.rotate(-90);
    p.drawText(0, 0, QString::number(s->scene.cpuPerc, 'f', 0) + "%");
    p.resetMatrix();
    if (s->scene.batt_less) {
      // sys temp
      sp_yr = sp_yr + j_num;
      if (s->scene.ambientTemp > 50) {
        p.setPen(redColor(200));
      } else if (s->scene.ambientTemp > 45) {
        p.setPen(orangeColor(200));
      } 
      debugText(p, sp_xr, sp_yr, QString::number(s->scene.ambientTemp, 'f', 0) + "°C", 150, 58);
      p.setPen(whiteColor(200));
      debugText(p, sp_xr, sp_yr + 35, QString("SYS TEMP"), 150, 27);
      p.translate(sp_xr + 90, sp_yr + 20);
      p.rotate(-90);
      p.drawText(0, 0, QString::number(s->scene.fanSpeed/1000, 'f', 0));
      p.resetMatrix();
    } else {
      // bat temp
      sp_yr = sp_yr + j_num;
      if (s->scene.batTemp > 50) {
        p.setPen(redColor(200));
      } else if (s->scene.batTemp > 40) {
        p.setPen(orangeColor(200));
      }
      debugText(p, sp_xr, sp_yr, QString::number(s->scene.batTemp, 'f', 0) + "°C", 150, 58);
      p.setPen(whiteColor(200));
      debugText(p, sp_xr, sp_yr + 35, QString("BAT TEMP"), 150, 27);
      p.translate(sp_xr + 90, sp_yr + 20);
      p.rotate(-90);
      p.drawText(0, 0, "  " + QString::number(s->scene.fanSpeed/1000, 'f', 0));
      p.resetMatrix();
      // bat lvl
      sp_yr = sp_yr + j_num;
      debugText(p, sp_xr, sp_yr, QString::number(s->scene.batPercent, 'f', 0) + "%", 150, 58);
      debugText(p, sp_xr, sp_yr + 35, QString("BAT LVL"), 150, 27);
      p.translate(sp_xr + 90, sp_yr + 20);
      p.rotate(-90);
      p.drawText(0, 0, s->scene.deviceState.getBatteryStatus() == "Charging" ? "   ++" : "   --");
      p.resetMatrix();
    }
    // Ublox GPS accuracy
    if (s->scene.gpsAccuracyUblox != 0.00) {
      sp_yr = sp_yr + j_num;
      if (s->scene.gpsAccuracyUblox > 1.3) {
        p.setPen(redColor(200));
      } else if (s->scene.gpsAccuracyUblox > 0.85) {
        p.setPen(orangeColor(200));
      }
      if (s->scene.gpsAccuracyUblox > 99 || s->scene.gpsAccuracyUblox == 0) {
        debugText(p, sp_xr, sp_yr, "None", 150, 58);
      } else if (s->scene.gpsAccuracyUblox > 9.99) {
        debugText(p, sp_xr, sp_yr, QString::number(s->scene.gpsAccuracyUblox, 'f', 1), 150, 58);
      } else {
        debugText(p, sp_xr, sp_yr, QString::number(s->scene.gpsAccuracyUblox, 'f', 2), 150, 58);
      }
      p.setPen(whiteColor(200));
      debugText(p, sp_xr, sp_yr + 35, QString("GPS PREC"), 150, 27);
      p.translate(sp_xr + 90, sp_yr + 20);
      p.rotate(-90);
      p.drawText(0, 0, QString::number(s->scene.satelliteCount, 'f', 0));
      p.resetMatrix();
      // altitude
      sp_yr = sp_yr + j_num;
      debugText(p, sp_xr, sp_yr, QString::number(s->scene.altitudeUblox, 'f', 0), 150, 58);
      debugText(p, sp_xr, sp_yr + 35, QString("ALTITUDE"), 150, 27);
      p.translate(sp_xr + 90, sp_yr + 20);
      p.rotate(-90);
      p.drawText(0, 0, "m");
      p.resetMatrix();
    }
  }

  // opkr tpms
  if (!comma_stock_ui) {
    int tpms_width = 180;
    int tpms_sp_xr = rect().right() - bdr_s - tpms_width / 2;
    int tpms_sp_yr = rect().bottom() - bdr_s - 260;
    QRect tpms_panel(rect().right() - bdr_s - tpms_width, tpms_sp_yr - 20, tpms_width, 130);  
    p.setOpacity(1.0);
    p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    p.drawRoundedRect(tpms_panel, 20, 20);
    p.setPen(QColor(255, 255, 255, 200));
    //p.setRenderHint(QPainter::TextAntialiasing);
    float maxv = 0;
    float minv = 300;

    if (maxv < s->scene.tpmsPressureFl) {maxv = s->scene.tpmsPressureFl;}
    if (maxv < s->scene.tpmsPressureFr) {maxv = s->scene.tpmsPressureFr;}
    if (maxv < s->scene.tpmsPressureRl) {maxv = s->scene.tpmsPressureRl;}
    if (maxv < s->scene.tpmsPressureRr) {maxv = s->scene.tpmsPressureRr;}
    if (minv > s->scene.tpmsPressureFl) {minv = s->scene.tpmsPressureFl;}
    if (minv > s->scene.tpmsPressureFr) {minv = s->scene.tpmsPressureFr;}
    if (minv > s->scene.tpmsPressureRl) {minv = s->scene.tpmsPressureRl;}
    if (minv > s->scene.tpmsPressureRr) {minv = s->scene.tpmsPressureRr;}

    if ((maxv - minv) > 3) {
      p.setBrush(QColor(255, 0, 0, 150));
    }
    debugText(p, tpms_sp_xr, tpms_sp_yr+15, "TPMS", 150, 33);
    if (s->scene.tpmsPressureFl < 32) {
      p.setPen(yellowColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+55, QString::number(s->scene.tpmsPressureFl, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    } else if (s->scene.tpmsPressureFl > 50) {
      p.setPen(whiteColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+55, "N/A", 150, (s->scene.tpmsUnit != 0?39:45));
    } else if (s->scene.tpmsPressureFl > 45) {
      p.setPen(redColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+55, QString::number(s->scene.tpmsPressureFl, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    } else {
      p.setPen(greenColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+55, QString::number(s->scene.tpmsPressureFl, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    }
    if (s->scene.tpmsPressureFr < 32) {
      p.setPen(yellowColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+55, QString::number(s->scene.tpmsPressureFr, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    } else if (s->scene.tpmsPressureFr > 50) {
      p.setPen(whiteColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+55, "N/A", 150, (s->scene.tpmsUnit != 0?39:45));
    } else if (s->scene.tpmsPressureFr > 45) {
      p.setPen(redColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+55, QString::number(s->scene.tpmsPressureFr, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    } else {
      p.setPen(greenColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+55, QString::number(s->scene.tpmsPressureFr, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    }
    if (s->scene.tpmsPressureRl < 32) {
      p.setPen(yellowColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+95, QString::number(s->scene.tpmsPressureRl, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    } else if (s->scene.tpmsPressureRl > 50) {
      p.setPen(whiteColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+95, "N/A", 150, (s->scene.tpmsUnit != 0?39:45));
    } else if (s->scene.tpmsPressureRl > 45) {
      p.setPen(redColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+95, QString::number(s->scene.tpmsPressureRl, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    } else {
      p.setPen(greenColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+95, QString::number(s->scene.tpmsPressureRl, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    }
    if (s->scene.tpmsPressureRr < 32) {
      p.setPen(yellowColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+95, QString::number(s->scene.tpmsPressureRr, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    } else if (s->scene.tpmsPressureRr > 50) {
      p.setPen(whiteColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+95, "N/A", 150, (s->scene.tpmsUnit != 0?39:45));
    } else if (s->scene.tpmsPressureRr > 45) {
      p.setPen(redColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+95, QString::number(s->scene.tpmsPressureRr, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    } else {
      p.setPen(greenColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+95, QString::number(s->scene.tpmsPressureRr, 'f', (s->scene.tpmsUnit != 0?1:0)), 150, (s->scene.tpmsUnit != 0?39:45));
    }
  }

  if (!comma_stock_ui) {
    // opkr rec
    QRect recbtn_draw(rect().right() - bdr_s - 140 - 20, 905, 140, 140);
    p.setBrush(Qt::NoBrush);
    if (record_stat) p.setBrush(redColor(150));
    p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    p.drawEllipse(recbtn_draw);
    p.setPen(whiteColor(200));
    p.drawText(recbtn_draw, Qt::AlignCenter, QString("REC"));

    // lane selector
    QRect lanebtn_draw(rect().right() - bdr_s - 140 - 20 - 160, 905, 140, 140);
    p.setBrush(Qt::NoBrush);
    if (laneless_stat) p.setBrush(greenColor(150));
    p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    p.drawEllipse(lanebtn_draw);
    p.setPen(whiteColor(200));
    if (lane_stat == 0) {
      configFont(p, "Open Sans", 39, "SemiBold");
      p.drawText(QRect(rect().right() - bdr_s - 140 - 20 - 160, 890, 140, 140), Qt::AlignCenter, QString("LANE"));
      p.drawText(QRect(rect().right() - bdr_s - 140 - 20 - 160, 920, 140, 140), Qt::AlignCenter, QString("LINE"));
    } else if (lane_stat == 1) {
      configFont(p, "Open Sans", 39, "SemiBold");
      p.drawText(QRect(rect().right() - bdr_s - 140 - 20 - 160, 890, 140, 140), Qt::AlignCenter, QString("LANE"));
      p.drawText(QRect(rect().right() - bdr_s - 140 - 20 - 160, 920, 140, 140), Qt::AlignCenter, QString("LESS"));
    } else if (lane_stat == 2) {
      p.drawText(lanebtn_draw, Qt::AlignCenter, QString("AUTO"));
    }

    // navi button
    QRect navibtn_draw(rect().right() - bdr_s - 140 - 20 - 160 - 160, 905, 140, 140);
    p.setBrush(Qt::NoBrush);
    if (map_stat) p.setBrush(blueColor(150));
    p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    p.drawEllipse(navibtn_draw);
    p.setPen(whiteColor(200));
    if (mapbox_stat) {
      configFont(p, "Open Sans", 38, "SemiBold");
      p.drawText(QRect(rect().right() - bdr_s - 140 - 20 - 160 - 160, 890, 140, 140), Qt::AlignCenter, QString("MAP"));
      p.drawText(QRect(rect().right() - bdr_s - 140 - 20 - 160 - 160, 920, 140, 140), Qt::AlignCenter, QString("Search"));
    } else {
      configFont(p, "Open Sans", 45, "SemiBold");
      p.drawText(navibtn_draw, Qt::AlignCenter, QString("NAVI"));
    }
  }
  //if (standstill) {
  if (true) {
    int minute = 0;
    int second = 0;
    minute = int(ss_elapsed / 60);
    second = int(ss_elapsed) - (minute * 60);
    p.setPen(ochreColor(240));
    debugText(p, mapbox_stat?(rect().right()-bdr_s-295):(rect().right()-bdr_s-545), bdr_s+410, "STOP", 240, mapbox_stat?105:150);
    p.setPen(whiteColor(240));
    debugText(p, mapbox_stat?(rect().right()-bdr_s-295):(rect().right()-bdr_s-545), mapbox_stat?bdr_s+510:bdr_s+560, QString::number(minute).rightJustified(2,'0') + ":" + QString::number(second).rightJustified(2,'0'), 240, mapbox_stat?125:175);
  }

}

void OnroadHud::drawText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void OnroadHud::debugText(QPainter &p, int x, int y, const QString &text, int alpha, int fontsize) {
  configFont(p, "Open Sans", fontsize, "SemiBold");
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  //p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void OnroadHud::uiText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x + real_rect.width() / 2, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void OnroadHud::drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity, bool rotation, float angle) {
  // opkr
  if (rotation) {
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(x - radius / 2, y - radius / 2, radius, radius);
    p.setOpacity(opacity);
    p.save();
    p.translate(x, y);
    p.rotate(-angle);
    QRect r = img.rect();
    r.moveCenter(QPoint(0,0));
    p.drawPixmap(r, img);
    p.restore();
  } else {
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(x - radius / 2, y - radius / 2, radius, radius);
    p.setOpacity(opacity);
    p.drawPixmap(x - img_size / 2, y - img_size / 2, img);
  }
}

// NvgWindow
void NvgWindow::initializeGL() {
  CameraViewWidget::initializeGL();
  qInfo() << "OpenGL version:" << QString((const char*)glGetString(GL_VERSION));
  qInfo() << "OpenGL vendor:" << QString((const char*)glGetString(GL_VENDOR));
  qInfo() << "OpenGL renderer:" << QString((const char*)glGetString(GL_RENDERER));
  qInfo() << "OpenGL language version:" << QString((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

  prev_draw_t = millis_since_boot();
  setBackgroundColor(bg_colors[STATUS_DISENGAGED]);
}

void NvgWindow::updateFrameMat(int w, int h) {
  CameraViewWidget::updateFrameMat(w, h);

  UIState *s = &QUIState::ui_state;
  s->fb_w = w;
  s->fb_h = h;
  auto intrinsic_matrix = s->wide_camera ? ecam_intrinsic_matrix : fcam_intrinsic_matrix;
  float zoom = ZOOM / intrinsic_matrix.v[0];
  if (s->wide_camera) {
    zoom *= 0.5;
  }
  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  // 2) Apply same scaling as video
  // 3) Put (0, 0) in top left corner of video
  s->car_space_transform.reset();
  s->car_space_transform.translate(w / 2, h / 2 + y_offset)
      .scale(zoom, zoom)
      .translate(-intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);
}

void NvgWindow::drawLaneLines(QPainter &painter, const UIScene &scene) {
  int steerOverride = scene.car_state.getSteeringPressed();
  float steer_max_v = scene.steerMax_V - (1.5 * (scene.steerMax_V - 0.9));
  int torque_scale = (int)fabs(255*(float)scene.output_scale*steer_max_v);
  int red_lvl = fmin(255, torque_scale);
  int green_lvl = fmin(255, 255-torque_scale);
  float red_lvl_line = 1.0;
  float green_lvl_line = 1.0;

  // opkr  
  if (!scene.lateralPlan.lanelessModeStatus) {
    // lanelines + hoya's colored lane line
    for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
      if (scene.lane_line_probs[i] > 0.4){
        red_lvl_line = fmin(1.0, 1.0 - ((scene.lane_line_probs[i] - 0.4) * 2.5));
        green_lvl_line = 1.0;
      } else {
        red_lvl_line = 1.0;
        green_lvl_line = fmin(1.0, 1.0 - ((0.4 - scene.lane_line_probs[i]) * 2.5));
      }
      if (!scene.comma_stock_ui) {
        painter.setBrush(QColor::fromRgbF(fmax(0.0, red_lvl_line), fmax(0.0, green_lvl_line), 0.0, 1.0));
        painter.drawPolygon(scene.lane_line_vertices[i].v, scene.lane_line_vertices[i].cnt);
      } else {
        painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, scene.lane_line_probs[i]));
        painter.drawPolygon(scene.lane_line_vertices[i].v, scene.lane_line_vertices[i].cnt);
      }
    }
    // road edges
    for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
      painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
      painter.drawPolygon(scene.road_edge_vertices[i].v, scene.road_edge_vertices[i].cnt);
    }
  }
  // paint path
  QLinearGradient bg(0, height(), 0, height() / 4);
  if (scene.controls_state.getEnabled() && !scene.comma_stock_ui) {
    if (steerOverride) {
      bg.setColorAt(0, blackColor(80));
      bg.setColorAt(1, blackColor(20));
      painter.setBrush(bg);
      painter.drawPolygon(scene.track_vertices.v, scene.track_vertices.cnt);
    } else {
      bg.setColorAt(0, QColor(red_lvl, green_lvl, 0, 150));
      bg.setColorAt(1, QColor(int(0.7*red_lvl), int(0.7*green_lvl), 0, 20));
      painter.setBrush(bg);
      painter.drawPolygon(scene.track_vertices.v, scene.track_vertices.cnt);
    }
  } else {
    bg.setColorAt(0, whiteColor(150));
    bg.setColorAt(1, whiteColor(20));
    painter.setBrush(bg);
    painter.drawPolygon(scene.track_vertices.v, scene.track_vertices.cnt);
  }
}

void NvgWindow::drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd) {
  UIState *s = &QUIState::ui_state;
  float fillAlpha = 0;
  float speedBuff = 10.;
  float leadBuff = 40.;
  float d_rel = lead_data.getDRel();
  float v_rel = lead_data.getVRel();
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  float y = std::fmin(height() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  // opkr
  if (s->scene.radarDistance < 149) {
    QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_xo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
    painter.setBrush(QColor(218, 202, 37, 255));
    painter.drawPolygon(glow, std::size(glow));

    // chevron
    QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
    painter.setBrush(redColor(fillAlpha));
    painter.drawPolygon(chevron, std::size(chevron));
    painter.setPen(QColor(0xff, 0xff, 0xff));
    //painter.setRenderHint(QPainter::TextAntialiasing);
    configFont(painter, "Open Sans", 35, "SemiBold");
    painter.drawText(QRect(x - (sz * 1.25), y, 2 * (sz * 1.25), sz * 1.25), Qt::AlignCenter, QString("R")); // opkr
  } else {
    QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_xo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
    painter.setBrush(QColor(0, 255, 0, 255));
    painter.drawPolygon(glow, std::size(glow));

    // chevron
    QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
    painter.setBrush(greenColor(fillAlpha));
    painter.drawPolygon(chevron, std::size(chevron));
    painter.setPen(QColor(0x0, 0x0, 0x0));
    //painter.setRenderHint(QPainter::TextAntialiasing);
    configFont(painter, "Open Sans", 35, "SemiBold");
    painter.drawText(QRect(x - (sz * 1.25), y, 2 * (sz * 1.25), sz * 1.25), Qt::AlignCenter, QString("V")); // opkr
  }
}

void NvgWindow::paintGL() {
  CameraViewWidget::paintGL();

  UIState *s = &QUIState::ui_state;
  if (s->scene.world_objects_visible) {
    QPainter painter(this);
    //painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    drawLaneLines(painter, s->scene);

    if (true) {
      auto lead_one = (*s->sm)["radarState"].getRadarState().getLeadOne();
      auto lead_two = (*s->sm)["radarState"].getRadarState().getLeadTwo();
      if (lead_one.getStatus()) {
        drawLead(painter, lead_one, s->scene.lead_vertices[0]);
      }
      if (lead_two.getStatus() && (std::abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0)) {
        drawLead(painter, lead_two, s->scene.lead_vertices[1]);
      }
    }
  }

  double cur_draw_t = millis_since_boot();
  double dt = cur_draw_t - prev_draw_t;
  if (dt > 66) {
    // warn on sub 15fps
    LOGW("slow frame time: %.2f", dt);
  }
  prev_draw_t = cur_draw_t;
}

void NvgWindow::showEvent(QShowEvent *event) {
  CameraViewWidget::showEvent(event);

  ui_update_params(&QUIState::ui_state);
  prev_draw_t = millis_since_boot();
}
