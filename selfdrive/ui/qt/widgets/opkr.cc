#include "selfdrive/ui/qt/widgets/opkr.h"

#include <QHBoxLayout>
#include <QTextStream>
#include <QFile>

#include "selfdrive/common/params.h"
#include "selfdrive/ui/qt/widgets/input.h"

CarSelectCombo::CarSelectCombo() : AbstractControl("", "", "") 
{
  combobox.setStyleSheet(R"(
    subcontrol-origin: padding;
    subcontrol-position: top left;
    selection-background-color: #111;
    selection-color: yellow;
    color: white;
    background-color: #393939;
    border-style: solid;
    border: 0px solid #1e1e1e;
    border-radius: 0;
    width: 100px;
  )");

  combobox.addItem("Select Your Car");
  QFile carlistfile("/data/params/d/CarList");
  if (carlistfile.open(QIODevice::ReadOnly)) {
    QTextStream carname(&carlistfile);
    while (!carname.atEnd()) {
      QString line = carname.readLine();
      combobox.addItem(line);
    }
    carlistfile.close();
  }

  combobox.setFixedWidth(1055);

  btn.setStyleSheet(R"(
    padding: 0;
    border-radius: 50px;
    font-size: 35px;
    font-weight: 500;
    color: #E4E4E4;
    background-color: #393939;
  )");

  btn.setFixedSize(150, 100);

  QObject::connect(&btn, &QPushButton::clicked, [=]() {
    if (btn.text() == "UNSET") {
      if (ConfirmationDialog::confirm("Do you want to unset?", this)) {
        params.remove("CarModel");
        combobox.setCurrentIndex(0);
        refresh();
      }
    }
  });

  //combobox.view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  hlayout->addWidget(&combobox);
  hlayout->addWidget(&btn, Qt::AlignRight);

  QObject::connect(&combobox, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), [=](int index)
  {
    combobox.itemData(combobox.currentIndex());
    QString str = combobox.currentText();
    if (combobox.currentIndex() != 0) {
      if (ConfirmationDialog::confirm("Press OK to set your car as\n" + str, this)) {
        params.put("CarModel", str.toStdString());
      }
    }
    refresh();
  });
  refresh();
}

void CarSelectCombo::refresh() {
  QString selected_carname = QString::fromStdString(params.get("CarModel"));
  int index = combobox.findText(selected_carname);
  if (index >= 0) combobox.setCurrentIndex(index);
  if (selected_carname.length()) {
    btn.setEnabled(true);
    btn.setText("UNSET");
  } else {
    btn.setEnabled(false);
    btn.setText("SET");
  }
}

OpenpilotView::OpenpilotView() : AbstractControl("Openpilot Camera", "This will preview openpilot onroad camera.", "") {

  // setup widget
  hlayout->addStretch(1);

  btn.setStyleSheet(R"(
    padding: 0;
    border-radius: 50px;
    font-size: 35px;
    font-weight: 500;
    color: #E4E4E4;
    background-color: #393939;
  )");

  btn.setFixedSize(250, 100);
  hlayout->addWidget(&btn);

  QObject::connect(&btn, &QPushButton::clicked, [=]() {
    bool stat = params.getBool("IsOpenpilotViewEnabled");
    if (stat) {
      params.putBool("IsOpenpilotViewEnabled", false);
    } else {
      params.putBool("IsOpenpilotViewEnabled", true);
    }
    refresh();
  });
  refresh();
}

void OpenpilotView::refresh() {
  bool param = params.getBool("IsOpenpilotViewEnabled");
  QString car_param = QString::fromStdString(params.get("CarParams"));
  if (param) {
    btn.setText("EXIT");
  } else {
    btn.setText("PREVIEW");
  }
  if (car_param.length()) {
    btn.setEnabled(false);
  } else {
    btn.setEnabled(true);
  }
}