#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "scopewidget.h"
#include "spectrumwidget.h"

#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    const QStringList sources = ui->scopeWidget->deviceNames();
    ui->sourceCombo->addItems(sources);

    ui->channelCombo->addItem(QStringLiteral("Stereo"), ScopeWidget::ChannelStereo);
    ui->channelCombo->addItem(QStringLiteral("Left"), ScopeWidget::ChannelLeft);
    ui->channelCombo->addItem(QStringLiteral("Right"), ScopeWidget::ChannelRight);

    connect(ui->sourceCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        ui->scopeWidget->setDeviceIndex(index);
    });

    connect(ui->channelCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        const int mode = ui->channelCombo->itemData(index).toInt();
        ui->scopeWidget->setChannelMode(static_cast<ScopeWidget::ChannelMode>(mode));
    });

    connect(ui->timeScaleSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        ui->scopeWidget->setTimeScaleMs(value);
    });

    connect(ui->startButton, &QPushButton::clicked, this, [this]() {
        if (ui->scopeWidget->isCapturing()) {
            ui->scopeWidget->stopCapture();
            ui->startButton->setText(QStringLiteral("Start"));
        } else {
            if (ui->scopeWidget->startCapture()) {
                ui->startButton->setText(QStringLiteral("Stop"));
            }
        }
    });

    connect(ui->scopeWidget, &ScopeWidget::statusChanged, this, [this](const QString &text) {
        statusBar()->showMessage(text);
    });

    connect(ui->scopeWidget, &ScopeWidget::frameReady, ui->spectrumWidget, &SpectrumWidget::setSamples);

    if (sources.isEmpty()) {
        ui->startButton->setEnabled(false);
        statusBar()->showMessage(QStringLiteral("No capture devices found"));
    } else {
        statusBar()->showMessage(QStringLiteral("Ready"));
        if (ui->scopeWidget->startCapture()) {
            ui->startButton->setText(QStringLiteral("Stop"));
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
