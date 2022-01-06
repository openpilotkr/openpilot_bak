#include "selfdrive/ui/qt/widgets/prime.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <QrCode.hpp>

#include "selfdrive/common/params.h"
#include "selfdrive/ui/qt/request_repeater.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/qt_window.h"

using qrcodegen::QrCode;

PairingQRWidget::PairingQRWidget(QWidget* parent) : QWidget(parent) {
  QTimer* timer = new QTimer(this);
  timer->start(5 * 60 * 1000);
  connect(timer, &QTimer::timeout, this, &PairingQRWidget::refresh);
}

void PairingQRWidget::showEvent(QShowEvent *event) {
  refresh();
}

void PairingQRWidget::refresh() {
  Params params;
  QString IMEI = QString::fromStdString(params.get("IMEI"));
  QString serial = QString::fromStdString(params.get("HardwareSerial"));

  if (isVisible()) {
    QString pairToken = CommaApi::create_jwt({{"pair", true}});
    QString qrString = IMEI + "--" + serial + "--" + pairToken;
    this->updateQrCode(qrString);
  }
}

void PairingQRWidget::updateQrCode(const QString &text) {
  QrCode qr = QrCode::encodeText(text.toUtf8().data(), QrCode::Ecc::LOW);
  qint32 sz = qr.getSize();
  QImage im(sz, sz, QImage::Format_RGB32);

  QRgb black = qRgb(0, 0, 0);
  QRgb white = qRgb(255, 255, 255);
  for (int y = 0; y < sz; y++) {
    for (int x = 0; x < sz; x++) {
      im.setPixel(x, y, qr.getModule(x, y) ? black : white);
    }
  }

  // Integer division to prevent anti-aliasing
  int final_sz = ((width() / sz) - 1) * sz;
  img = QPixmap::fromImage(im.scaled(final_sz, final_sz, Qt::KeepAspectRatio), Qt::MonoOnly);
}

void PairingQRWidget::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  p.fillRect(rect(), Qt::white);

  QSize s = (size() - img.size()) / 2;
  p.drawPixmap(s.width(), s.height(), img);
}


PairingPopup::PairingPopup(QWidget *parent) : QDialogBase(parent) {
  QHBoxLayout *hlayout = new QHBoxLayout(this);
  hlayout->setContentsMargins(0, 0, 0, 0);
  hlayout->setSpacing(0);

  setStyleSheet("PairingPopup { background-color: #E0E0E0; }");

  // text
  QVBoxLayout *vlayout = new QVBoxLayout();
  vlayout->setContentsMargins(85, 70, 50, 70);
  vlayout->setSpacing(50);
  hlayout->addLayout(vlayout, 1);
  {
    QPushButton *close = new QPushButton(QIcon(":/icons/close.svg"), "", this);
    close->setIconSize(QSize(80, 80));
    close->setStyleSheet("border: none;");
    vlayout->addWidget(close, 0, Qt::AlignLeft);
    QObject::connect(close, &QPushButton::clicked, this, &QDialog::reject);

    vlayout->addSpacing(30);

    QLabel *title = new QLabel("Pair your device", this);
    title->setStyleSheet("font-size: 75px; color: black;");
    title->setWordWrap(true);
    vlayout->addWidget(title);

    QLabel *instructions = new QLabel(R"(
      <ol type='1' style='margin-left: 15px;'>
        <li style='margin-bottom: 50px;'>Go to API Server</li>
        <li style='margin-bottom: 50px;'>Join and Add the text_key using QR Code Scanner to pair your device</li>
      </ol>
    )", this);
    instructions->setStyleSheet("font-size: 47px; font-weight: bold; color: black;");
    instructions->setWordWrap(true);
    vlayout->addWidget(instructions);

    vlayout->addStretch();
  }

  // QR code
  PairingQRWidget *qr = new PairingQRWidget(this);
  hlayout->addWidget(qr, 1);
}


PrimeUserWidget::PrimeUserWidget(QWidget* parent) : QWidget(parent) {
  mainLayout = new QVBoxLayout(this);
  mainLayout->setMargin(30);

  QLabel* commaPrime = new QLabel("OPKR");
  mainLayout->addWidget(commaPrime, 0, Qt::AlignCenter);
  mainLayout->addSpacing(15);
  QPixmap hkgpix("../assets/addon/img/hkg.png");
  QLabel *hkg = new QLabel();
  hkg->setPixmap(hkgpix.scaledToWidth(470, Qt::SmoothTransformation));
  hkg->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
  mainLayout->addWidget(hkg, 0, Qt::AlignCenter);

  setStyleSheet(R"(
    QLabel {
      font-size: 70px;
      font-weight: 500;
    }
  )");
  setStyleSheet(R"(
    PrimeUserWidget {
      background-color: #333333;
      border-radius: 10px;
    }
  )");

  // set up API requests
  QString OPKR_SERVER = QString::fromStdString(Params().get("OPKRServer"));
  QString TARGET_SERVER = "";
  if (OPKR_SERVER == "0") {
    TARGET_SERVER = util::getenv("API_HOST", "https://api.retropilot.org").c_str();
  } else if (OPKR_SERVER == "1") {
    TARGET_SERVER = util::getenv("API_HOST", "https://api.commadotai.com").c_str();
  } else if (OPKR_SERVER == "2") {
    TARGET_SERVER = "http://" + QString::fromStdString(Params().get("OPKRServerAPI"));
  } else {
    TARGET_SERVER = util::getenv("API_HOST", "https://api.retropilot.org").c_str();
  }

  // set up API requests
  if (auto dongleId = getDongleId()) {
    QString url = TARGET_SERVER + "/v1/devices/" + dongleId + "/owner";
    RequestRepeater *repeater = new RequestRepeater(this, url, "ApiCache_Owner", 6);
    QObject::connect(repeater, &RequestRepeater::requestDone, this, &PrimeUserWidget::replyFinished);
  }
}

void PrimeUserWidget::replyFinished(const QString &response) {
  QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
  if (doc.isNull()) {
    qDebug() << "JSON Parse failed on getting points";
    return;
  }

  QJsonObject json = doc.object();
  points->setText(QString::number(json["points"].toInt()));
}

PrimeAdWidget::PrimeAdWidget(QWidget* parent) : QFrame(parent) {
  QVBoxLayout* main_layout = new QVBoxLayout(this);
  main_layout->setMargin(30);
  main_layout->setSpacing(15);

  main_layout->addWidget(new QLabel("OPKR"), 1, Qt::AlignCenter);

  QPixmap hkgpix("../assets/addon/img/hkg.png");
  QLabel *hkg = new QLabel();
  hkg->setPixmap(hkgpix.scaledToWidth(430, Qt::SmoothTransformation));
  hkg->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
  main_layout->addWidget(hkg, 0, Qt::AlignCenter);

  setStyleSheet(R"(
    PrimeAdWidget {
      border-radius: 10px;
      background-color: #333333;
    }
  )");
}


SetupWidget::SetupWidget(QWidget* parent) : QFrame(parent) {
  mainLayout = new QStackedWidget;

  // Unpaired, registration prompt layout

  QWidget* finishRegistration = new QWidget;
  finishRegistration->setObjectName("primeWidget");
  QVBoxLayout* finishRegistationLayout = new QVBoxLayout(finishRegistration);
  finishRegistationLayout->setMargin(30);
  finishRegistationLayout->setSpacing(10);

  QPixmap hkgpix("../assets/addon/img/hkg.png");
  QLabel *hkg = new QLabel();
  hkg->setPixmap(hkgpix.scaledToWidth(450, Qt::SmoothTransformation));
  hkg->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
  finishRegistationLayout->addWidget(hkg, 0, Qt::AlignCenter);

  QPushButton* pair = new QPushButton("Show QR Code");
  pair->setFixedHeight(150);
  pair->setStyleSheet(R"(
    QPushButton {
      font-size: 45px;
      font-weight: 500;
      border-radius: 10px;
      background-color: #465BEA;
    }
    QPushButton:pressed {
      background-color: #3049F4;
    }
  )");
  finishRegistationLayout->addWidget(pair);

  popup = new PairingPopup(this);
  QObject::connect(pair, &QPushButton::clicked, popup, &PairingPopup::exec);

  mainLayout->addWidget(finishRegistration);

  // build stacked layout
  QVBoxLayout *outer_layout = new QVBoxLayout(this);
  outer_layout->setContentsMargins(0, 0, 0, 0);
  outer_layout->addWidget(mainLayout);

  primeAd = new PrimeAdWidget;
  mainLayout->addWidget(primeAd);

  primeUser = new PrimeUserWidget;
  mainLayout->addWidget(primeUser);

  mainLayout->setCurrentWidget(primeAd);

  setFixedWidth(750);
  setStyleSheet(R"(
    #primeWidget {
      border-radius: 10px;
      background-color: #333333;
    }
  )");

  // Retain size while hidden
  QSizePolicy sp_retain = sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  setSizePolicy(sp_retain);

  // set up API requests
  QString OPKR_SERVER = QString::fromStdString(Params().get("OPKRServer"));
  QString TARGET_SERVER = "";
  if (OPKR_SERVER == "0") {
    TARGET_SERVER = util::getenv("API_HOST", "https://api.retropilot.org").c_str();
  } else if (OPKR_SERVER == "1") {
    TARGET_SERVER = util::getenv("API_HOST", "https://api.commadotai.com").c_str();
  } else if (OPKR_SERVER == "2") {
    TARGET_SERVER = "http://" + QString::fromStdString(Params().get("OPKRServerAPI"));
  } else {
    TARGET_SERVER = util::getenv("API_HOST", "https://api.retropilot.org").c_str();
  }
  
  if (auto dongleId = getDongleId()) {
    QString url = TARGET_SERVER + "/v1.1/devices/" + *dongleId + "/";
    RequestRepeater* repeater = new RequestRepeater(this, url, "ApiCache_Device", 5);

    QObject::connect(repeater, &RequestRepeater::requestDone, this, &SetupWidget::replyFinished);
  }
  hide(); // Only show when first request comes back
}

void SetupWidget::replyFinished(const QString &response, bool success) {
  show();
  if (!success) return;

  QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
  if (doc.isNull()) {
    qDebug() << "JSON Parse failed on getting pairing and prime status";
    return;
  }

  QJsonObject json = doc.object();
  if (!json["is_paired"].toBool()) {
    mainLayout->setCurrentIndex(0);
  } else {
    popup->reject();

    bool prime = json["prime"].toBool();

    if (uiState()->has_prime != prime) {
      uiState()->has_prime = prime;
      Params().putBool("HasPrime", prime);
    }

    if (prime) {
      mainLayout->setCurrentWidget(primeUser);
    } else {
      mainLayout->setCurrentWidget(primeAd);
    }
  }
}
