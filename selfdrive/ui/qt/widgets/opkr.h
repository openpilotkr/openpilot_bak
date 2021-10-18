#pragma once

#include <QPushButton>

#include "selfdrive/hardware/hw.h"
#include "selfdrive/ui/qt/widgets/controls.h"
#include <QComboBox>
#include <QAbstractItemView>

class SshLegacyToggle : public ToggleControl {
  Q_OBJECT

public:
  SshLegacyToggle() : ToggleControl("Use Legacy SSH Key", "", "", Params().getBool("OpkrSSHLegacy")) {
    QObject::connect(this, &SshLegacyToggle::toggleFlipped, [=](int state) {
      bool status = state ? true : false;
      Params().putBool("OpkrSSHLegacy", status);
    });
  }
};

class CarSelectCombo : public AbstractControl 
{
  Q_OBJECT

public:
  CarSelectCombo();

private:
  QPushButton btn;
  QComboBox combobox;
  Params params;

  void refresh();
};

class PrebuiltToggle : public ToggleControl {
  Q_OBJECT

public:
  PrebuiltToggle() : ToggleControl("Use Prebuilt", "", "", Params().getBool("PutPrebuiltOn")) {
    QObject::connect(this, &PrebuiltToggle::toggleFlipped, [=](int state) {
      bool status = state ? true : false;
      Params().putBool("PutPrebuiltOn", status);
    });
  }
};

class OpenpilotView : public AbstractControl {
  Q_OBJECT

public:
  OpenpilotView();

private:
  QPushButton btn;
  Params params;
  
  void refresh();
};