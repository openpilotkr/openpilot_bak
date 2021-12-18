#pragma once

#include <QStackedLayout>
#include <QWidget>

#include "selfdrive/ui/qt/widgets/cameraview.h"
#include "selfdrive/ui/ui.h"


// ***** onroad widgets *****

class OnroadHud : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QString speed MEMBER speed NOTIFY valueChanged);
  Q_PROPERTY(QString speedUnit MEMBER speedUnit NOTIFY valueChanged);
  Q_PROPERTY(QString maxSpeed MEMBER maxSpeed NOTIFY valueChanged);
  Q_PROPERTY(QString cruiseSpeed MEMBER cruiseSpeed NOTIFY valueChanged);
  Q_PROPERTY(bool is_cruise_set MEMBER is_cruise_set NOTIFY valueChanged);
  Q_PROPERTY(bool engageable MEMBER engageable NOTIFY valueChanged);
  Q_PROPERTY(bool dmActive MEMBER dmActive NOTIFY valueChanged);
  Q_PROPERTY(bool hideDM MEMBER hideDM NOTIFY valueChanged);
  Q_PROPERTY(int status MEMBER status NOTIFY valueChanged);
  Q_PROPERTY(bool is_over_sl MEMBER is_over_sl NOTIFY valueChanged);
  Q_PROPERTY(bool comma_stock_ui MEMBER comma_stock_ui NOTIFY valueChanged);
  Q_PROPERTY(bool lead_stat MEMBER lead_stat NOTIFY valueChanged);
  Q_PROPERTY(float dist_rel MEMBER dist_rel NOTIFY valueChanged);
  Q_PROPERTY(float vel_rel MEMBER vel_rel NOTIFY valueChanged);
  Q_PROPERTY(float ang_str MEMBER ang_str NOTIFY valueChanged);
  Q_PROPERTY(bool record_stat MEMBER record_stat NOTIFY valueChanged);
  Q_PROPERTY(int lane_stat MEMBER lane_stat NOTIFY valueChanged);
  Q_PROPERTY(bool laneless_stat MEMBER laneless_stat NOTIFY valueChanged);
  Q_PROPERTY(bool map_stat MEMBER map_stat NOTIFY valueChanged);
  Q_PROPERTY(bool mapbox_stat MEMBER mapbox_stat NOTIFY valueChanged);
  Q_PROPERTY(bool dm_mode MEMBER dm_mode NOTIFY valueChanged);

public:
  explicit OnroadHud(QWidget *parent);
  void updateState(const UIState &s);

protected:
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }
  inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }
  inline QColor yellowColor(int alpha = 255) { return QColor(218, 202, 37, alpha); }
  inline QColor ochreColor(int alpha = 255) { return QColor(218, 111, 37, alpha); }
  inline QColor greenColor(int alpha = 255) { return QColor(0, 255, 0, alpha); }
  inline QColor blueColor(int alpha = 255) { return QColor(0, 0, 255, alpha); }
  inline QColor orangeColor(int alpha = 255) { return QColor(255, 175, 3, alpha); }
  inline QColor greyColor(int alpha = 1) { return QColor(191, 191, 191, alpha); }

private:
  void drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity, bool rotation = false, float angle = 0);
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255);
  void uiText(QPainter &p, int x, int y, const QString &text, int alpha = 255);
  void debugText(QPainter &p, int x, int y, const QString &text, int alpha = 255, int fontsize = 30);
  void paintEvent(QPaintEvent *event) override;

  QPixmap engage_img;
  QPixmap dm_img;
  const int radius = 180;
  const int img_size = (radius / 2) * 1.5;
  QString speed;
  QString speedUnit;
  QString maxSpeed;
  QString cruiseSpeed;
  bool is_cruise_set = false;
  bool engageable = false;
  bool dmActive = false;
  bool hideDM = false;
  int status = STATUS_DISENGAGED;
  bool is_over_sl = false;
  bool comma_stock_ui = false;
  bool lead_stat = false;
  float dist_rel = 0;
  float vel_rel = 0;
  float ang_str = 0;
  bool record_stat = false;
  int lane_stat = 0;
  bool laneless_stat = false;
  bool map_stat = false;
  bool mapbox_stat = false;
  bool dm_mode = false;

signals:
  void valueChanged();
};

class OnroadAlerts : public QWidget {
  Q_OBJECT

public:
  OnroadAlerts(QWidget *parent = 0) : QWidget(parent) {};
  void updateAlert(const Alert &a, const QColor &color);

protected:
  void paintEvent(QPaintEvent*) override;

private:
  QColor bg;
  Alert alert = {};
};

// container window for the NVG UI
class NvgWindow : public CameraViewWidget {
  Q_OBJECT

public:
  explicit NvgWindow(VisionStreamType type, QWidget* parent = 0) : CameraViewWidget("camerad", type, true, parent) {}

private:
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255);

protected:
  void paintGL() override;
  void initializeGL() override;
  void showEvent(QShowEvent *event) override;
  void updateFrameMat(int w, int h) override;
  void drawLaneLines(QPainter &painter, const UIScene &scene);
  void drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd);
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }
  inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }
  inline QColor yellowColor(int alpha = 255) { return QColor(218, 202, 37, alpha); }
  inline QColor ochreColor(int alpha = 255) { return QColor(218, 111, 37, alpha); }
  inline QColor greenColor(int alpha = 255) { return QColor(0, 255, 0, alpha); }
  inline QColor blueColor(int alpha = 255) { return QColor(0, 0, 255, alpha); }
  inline QColor orangeColor(int alpha = 255) { return QColor(255, 175, 3, alpha); }
  inline QColor greyColor(int alpha = 1) { return QColor(191, 191, 191, alpha); }
  double prev_draw_t = 0;
};

// container for all onroad widgets
class OnroadWindow : public QWidget {
  Q_OBJECT

public:
  OnroadWindow(QWidget* parent = 0);
  bool isMapVisible() const { return map && map->isVisible(); }

private:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent* e) override;
  OnroadHud *hud;
  OnroadAlerts *alerts;
  NvgWindow *nvg;
  QColor bg = bg_colors[STATUS_DISENGAGED];
  QWidget *map = nullptr;
  QHBoxLayout* split;

signals:
  void updateStateSignal(const UIState &s);
  void offroadTransitionSignal(bool offroad);

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);
};
