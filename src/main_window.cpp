#include "main_window.h"
#include "flat_cap.h"
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QUuid>
#include <QVBoxLayout>
#include <chrono>

namespace mw {
    MainWindow::MainWindow(QWidget *p) : QMainWindow(p) {
        setWindowTitle("MedicalWand — cavity acquisition");
        resize(1240, 820);
        clock_.start();
        session_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        auto root = new QWidget;
        auto layout = new QVBoxLayout(root);
        layout->setContentsMargins(18, 14, 18, 14);
        layout->setSpacing(12);
        setCentralWidget(root);

        auto header = new QHBoxLayout;
        auto title = new QLabel("MedicalWand");
        title->setStyleSheet("font-size:24px;font-weight:600;color:#edf5fa");
        header->addWidget(title);
        auto subtitle = new QLabel("CAVITY ACQUISITION  /  DEVELOPMENT");
        subtitle->setStyleSheet("color:#8ea8bb;font-size:11px");
        header->addWidget(subtitle);
        header->addStretch();
        auto button = [](const QString &text, const char *name, QHBoxLayout *row) {
            auto b = new QPushButton(text);
            b->setObjectName(name);
            row->addWidget(b);
            return b;
        };

        auto fresh = button("New session", "newSession", header);
        connect(fresh, &QPushButton::clicked, this, &MainWindow::newSession);
        auto open = button("Open…", "openSession", header);
        connect(open, &QPushButton::clicked, this, &MainWindow::load);
        auto saveButton = button("Save…", "saveSession", header);
        connect(saveButton, &QPushButton::clicked, this, [this] {
            save();
        });

        layout->addLayout(header);
        auto split = new QSplitter;
        layout->addWidget(split, 1);

        auto viewportPanel = new QWidget;
        auto viewportLayout = new QVBoxLayout(viewportPanel);
        viewportLayout->setContentsMargins(0, 0, 0, 0);

        auto tools = new QHBoxLayout;
        auto fitButton = button("Fit view", "fitView", tools);
        connect(fitButton, &QPushButton::clicked, this, [this] {
            viewer_->fit();
        });

        auto views = new QComboBox;
        views->addItems({"Perspective", "Top (XY)", "Front (XZ)", "Side (YZ)"});
        tools->addWidget(views);
        pointLayer_ = new QCheckBox("Points");
        pointLayer_->setObjectName("pointsLayer");
        pointLayer_->setChecked(true);
        tools->addWidget(pointLayer_);
        surfaceLayer_ = new QCheckBox("Estimated surface");
        surfaceLayer_->setChecked(true);
        tools->addWidget(surfaceLayer_);
        tools->addStretch();
        viewportLayout->addLayout(tools);
        viewer_ = new Viewer;
        viewportLayout->addWidget(viewer_, 1);
        split->addWidget(viewportPanel);
        connect(views, &QComboBox::currentIndexChanged, viewer_, &Viewer::standardView);
        auto layers = [this] {
            viewer_->setLayers(pointLayer_->isChecked(), surfaceLayer_->isChecked());
        };

        connect(pointLayer_, &QCheckBox::toggled, this, layers);
        connect(surfaceLayer_, &QCheckBox::toggled, this, layers);

        auto side = new QWidget;
        auto sideLayout = new QVBoxLayout(side);
        sideLayout->setContentsMargins(12, 0, 0, 0);
        sideLayout->setSpacing(12);
        auto group = [&](const QString &name) {
            auto box = new QGroupBox(name);
            auto l = new QVBoxLayout(box);
            sideLayout->addWidget(box);
            return l;
        };

        auto input = group("01   INPUT");
        source_ = new QComboBox;
        source_->setObjectName("source");
        source_->addItems(
            {"Simulated XYZ", "Simulated joint angles", "Pi SPI — not configured", "Replay loaded session"});
        input->addWidget(source_);
        sourceStatus_ = new QLabel;
        sourceStatus_->setObjectName("sourceStatus");
        sourceStatus_->setWordWrap(true);
        sourceStatus_->setStyleSheet("color:#f6c66c");
        input->addWidget(sourceStatus_);

        auto xyzPanel = new QWidget;
        auto xyz = new QGridLayout(xyzPanel);
        xyz->setContentsMargins(0, 0, 0, 0);
        const char *xyzNames[]{"X", "Y", "Z"};

        for (int i = 0; i < 3; ++i) {
            xyz->addWidget(new QLabel(QString(xyzNames[i]) + " (mm)"), i, 0);
            xyzControls_[i] = new QDoubleSpinBox;
            xyzControls_[i]->setRange(-500, 500);
            xyzControls_[i]->setDecimals(3);
            xyzControls_[i]->setSingleStep(0.5);
            xyz->addWidget(xyzControls_[i], i, 1);
            connect(xyzControls_[i], &QDoubleSpinBox::valueChanged, this, [this] {
                updateReading();
            });
        }

        input->addWidget(xyzPanel);

        auto jointPanel = new QWidget;
        auto joints = new QGridLayout(jointPanel);
        joints->setContentsMargins(0, 0, 0, 0);
        jointPanel->hide();
        const char *jointNames[]{"Base yaw", "Shoulder pitch", "Elbow pitch", "Forearm roll", "Wrist pitch"};

        for (int i = 0; i < 5; ++i) {
            joints->addWidget(new QLabel(jointNames[i]), i, 0);
            jointControls_[i] = new QDoubleSpinBox;
            jointControls_[i]->setRange(-180, 180);
            jointControls_[i]->setDecimals(2);
            jointControls_[i]->setSuffix("°");
            jointControls_[i]->setEnabled(false);
            joints->addWidget(jointControls_[i], i, 1);
            connect(jointControls_[i], &QDoubleSpinBox::valueChanged, this, [this] {
                updateReading();
            });
        }

        input->addWidget(jointPanel);
        auto replayButton = new QPushButton("Next replay sample");
        input->addWidget(replayButton);
        connect(replayButton, &QPushButton::clicked, this, &MainWindow::replayNext);
        replayButton->setEnabled(false);

        auto acquire = group("02   CAPTURE");
        position_ = new QLabel;
        position_->setStyleSheet("font-family:Consolas,monospace;font-size:13px;color:#e1eef7");
        acquire->addWidget(position_);
        capture_ = new QPushButton("Capture point   [Space]");
        capture_->setObjectName("capture");
        capture_->setStyleSheet("background:#197e79;color:white;font-weight:600;padding:12px");
        acquire->addWidget(capture_);
        connect(capture_, &QPushButton::clicked, this, &MainWindow::capture);
        auto undo = new QPushButton("Undo last capture");
        undo->setObjectName("undo");
        acquire->addWidget(undo);
        connect(undo, &QPushButton::clicked, this, [this] {
            if (!session_.captures.points.empty()) {
                session_.volumeTestBowl = false;
                session_.captures.points.pop_back();
                changed();
            }
        });

        demo_ = new QPushButton("Start demo scan");
        demo_->setObjectName("demo");
        acquire->addWidget(demo_);
        connect(demo_, &QPushButton::clicked, this, [this] {
            if (demoTimer_.isActive()) {
                demoTimer_.stop();
                demo_->setText("Start demo scan");
            } else {
                demoTimer_.start(33);
                demo_->setText("Stop demo scan");
            }
        });

        connect(source_, &QComboBox::currentIndexChanged, this,
            [this, replayButton, xyzPanel, jointPanel](int mode) {
                demoTimer_.stop();
                demo_->setText("Start demo scan");

                for (auto c : xyzControls_)
                    c->setEnabled(mode == 0);

                for (auto c : jointControls_)
                    c->setEnabled(mode == 1);
                xyzPanel->setVisible(mode == 0);
                jointPanel->setVisible(mode == 1);
                replayButton->setVisible(mode == 3);
                replayButton->setEnabled(mode == 3);
                demo_->setEnabled(mode == 0);
                updateReading();
            });

        replayButton->hide();
        pointCount_ = new QLabel;
        pointCount_->setObjectName("pointCount");
        acquire->addWidget(pointCount_);

        auto surface = group("03   SURFACE PREVIEW");
        reconstructionMode_ = new QComboBox;
        reconstructionMode_->setObjectName("reconstructionMode");
        reconstructionMode_->addItems({"Radial cavity (+Z opening)", "Local surface preview"});
        reconstructionMode_->setToolTip(
            "Radial mode assumes one wall per direction around the cloud center, a single opening along +Z, "
            "and full-around wall/floor coverage. It cannot represent hidden undercuts or multiple layers. "
            "Local mode preserves unsupported topology as gaps.");
        surface->addWidget(reconstructionMode_);
        auto settings = new QGridLayout;
        merge_ = new QDoubleSpinBox;
        merge_->setRange(0.01, 50);
        merge_->setDecimals(2);
        merge_->setValue(session_.settings.mergeDistance);
        edge_ = new QDoubleSpinBox;
        edge_->setRange(0.02, 100);
        edge_->setDecimals(2);
        edge_->setValue(session_.settings.maxEdge);
        settings->addWidget(new QLabel("Merge distance (mm)"), 0, 0);
        settings->addWidget(merge_, 0, 1);
        settings->addWidget(new QLabel("Max. connection (mm)"), 1, 0);
        settings->addWidget(edge_, 1, 1);
        surface->addLayout(settings);
        flatCap_ = new QCheckBox("Close opening + estimated volume");
        flatCap_->setObjectName("flatCap");
        surface->addWidget(flatCap_);
        curvedCap_ = new QCheckBox("Curved lid (follow wall curvature)");
        curvedCap_->setObjectName("curvedCap");
        curvedCap_->setToolTip("Continues the nearby wall slope and curvature from a fixed rim. The interior "
                                "is estimated, not measured. Uncheck for a flat cap.");
        surface->addWidget(curvedCap_);
        capOpening_ = new QSpinBox;
        capOpening_->setRange(1, 10000);
        capOpening_->setPrefix("Opening # ");
        capOpening_->setToolTip("Openings are ranked by rim perimeter, largest first. Inspect the orange cap "
                                "before using volume.");
        surface->addWidget(capOpening_);
        capTolerance_ = new QDoubleSpinBox;
        capTolerance_->setRange(0, 10);
        capTolerance_->setDecimals(3);
        capTolerance_->setValue(0.5);
        capTolerance_->setPrefix("Rim tolerance: ");
        capTolerance_->setSuffix(" mm");
        capTolerance_->setToolTip("Maximum displacement of derived rim vertices onto the fitted cap plane. "
                                    "Raw captures are unchanged.");
        surface->addWidget(capTolerance_);
        auto testBowl = new QPushButton("Load cap test bowl");
        testBowl->setObjectName("testBowl");
        testBowl->setToolTip(
            "Synthetic half-ellipsoid: radius 38 mm, depth 15 mm. Tests a known clean mesh.");
        surface->addWidget(testBowl);
        connect(testBowl, &QPushButton::clicked, this, [this] {
            if (!confirmDiscard())
                return;
            demoTimer_.stop();
            demo_->setText("Start demo scan");
            session_.captures.points.clear();
            session_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            savePath_.clear();
            replay_.clear();
            replayIndex_ = 0;
            sequence_ = 0;
            auto fixture = makeCapTestBowl();

            for (auto p : fixture.vertices) {
                Snapshot sample;
                sample.position = p;
                sample.timestampUs = clock_.nsecsElapsed() / 1000 + clockOffset_;
                sample.sequence = sequence_++;
                sample.source = "SIMULATED VOLUME TEST BOWL";
                session_.captures.points.push_back(sample);
            }

            session_.volumeTestBowl = true;
            flatCap_->setChecked(true);
            capOpening_->setValue(1);
            capTolerance_->setValue(0.5);
            source_->setCurrentIndex(0);
            changed();
            viewer_->fit();
            statusBar()->showMessage("Synthetic test bowl loaded — no hardware measurements.");
        });

        auto settingsChanged = [this] {
            const auto previous = session_.settings;

            if (edge_->value() < 2 * merge_->value()) {
                QSignalBlocker blocker(edge_);
                edge_->setValue(2 * merge_->value());
            }

            session_.settings = {merge_->value(), edge_->value()};
            session_.settings.radialCavity = reconstructionMode_->currentIndex() == 0;
            session_.settings.flatCap = flatCap_->isChecked();
            session_.settings.curvedCap = curvedCap_->isChecked();
            session_.settings.capTolerance = capTolerance_->value();
            session_.settings.capOpening = capOpening_->value() - 1;
            capTolerance_->setEnabled(flatCap_->isChecked() && !curvedCap_->isChecked());
            curvedCap_->setEnabled(flatCap_->isChecked());
            capOpening_->setEnabled(flatCap_->isChecked());
            bool sameWallSettings = previous.mergeDistance == session_.settings.mergeDistance &&
                previous.maxEdge == session_.settings.maxEdge &&
                previous.radialCavity == session_.settings.radialCavity;
            changed(true, sameWallSettings);
        };

        connect(merge_, &QDoubleSpinBox::valueChanged, this, settingsChanged);
        connect(edge_, &QDoubleSpinBox::valueChanged, this, settingsChanged);
        connect(flatCap_, &QCheckBox::toggled, this, settingsChanged);
        connect(curvedCap_, &QCheckBox::toggled, this, settingsChanged);
        connect(capTolerance_, &QDoubleSpinBox::valueChanged, this, settingsChanged);
        connect(capOpening_, &QSpinBox::valueChanged, this, settingsChanged);
        connect(reconstructionMode_, &QComboBox::currentIndexChanged, this, settingsChanged);
        capTolerance_->setEnabled(false);
        curvedCap_->setEnabled(false);
        capOpening_->setEnabled(false);
        meshStatus_ = new QLabel;
        meshStatus_->setObjectName("meshStatus");
        meshStatus_->setWordWrap(true);
        surface->addWidget(meshStatus_);
        auto notice = new QLabel("Radial mode: one wall per direction, opening along +Z. Hidden "
                                    "folds/undercuts need another reconstruction method. Orange = inferred "
                                    "cap. Volume is an estimate; insufficient coverage remains a gap.");
        notice->setWordWrap(true);
        notice->setStyleSheet("color:#8ea8bb");
        surface->addWidget(notice);

        auto exports = new QHBoxLayout;
        auto csv = button("Export CSV…", "exportCsv", exports);
        auto ply = button("Export PLY…", "exportPly", exports);
        surface->addLayout(exports);
        connect(csv, &QPushButton::clicked, this, [this] {
            QString path = QFileDialog::getSaveFileName(this, "Export raw points", {}, "CSV (*.csv)");

            if (path.isEmpty())
                return;
            QString message;

            if (!exportCsv(path, session_, message))
                error(message);
        });

        connect(ply, &QPushButton::clicked, this, [this] {
            QString path =
                QFileDialog::getSaveFileName(this, "Export current surface or raw points", {}, "PLY (*.ply)");

            if (path.isEmpty())
                return;
            QString message;

            if (!exportPly(path, session_, revision_.accepts(meshRevision_) ? &mesh_ : nullptr, message))
                error(message);
        });

        sideLayout->addStretch();

        auto scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setWidget(side);
        scroll->setMinimumWidth(325);
        split->addWidget(scroll);
        split->setSizes({850, 350});
        graphics_ = new QLabel("Checking graphics…");
        graphics_->setWordWrap(true);
        graphics_->setStyleSheet("color:#7793a8;font-size:10px");
        layout->addWidget(graphics_);
        connect(viewer_, &Viewer::graphicsStatus, this, [this](const QString &text) {
            graphics_->setText(text);
        });

        auto graphicsFailure = new QLabel("Graphics error: Qt could not create an OpenGL context.\nCheck "
                                            "graphics drivers and run from a graphical desktop.");
        graphicsFailure->setWordWrap(true);
        graphicsFailure->setObjectName("graphicsFailure");
        graphicsFailure->setStyleSheet("color:#ff9b8a;font-size:16px;padding:24px");
        graphicsFailure->hide();
        viewportLayout->insertWidget(1, graphicsFailure);
        QTimer::singleShot(1500, this, [this, graphicsFailure] {
            if (isVisible() && !viewer_->isValid()) {
                graphicsFailure->show();
                graphics_->setText("Graphics error: no OpenGL context available. Rendering is unavailable.");
                graphics_->setStyleSheet("color:#ff9b8a");
            }
        });

        setStyleSheet(
            "QMainWindow,QWidget{background:#101c28;color:#dce8f0;font-size:12px;}QGroupBox{border:1px solid "
            "#2d4252;border-radius:7px;margin-top:12px;padding:12px 8px "
            "8px;}QGroupBox::title{subcontrol-origin:margin;left:12px;color:#83a5b9;}QPushButton{background:#"
            "243848;border:1px solid "
            "#3a5162;border-radius:4px;padding:7px;}QPushButton:hover{background:#304c5e;}QPushButton:"
            "disabled{"
            "color:#607788;background:#1b2b38;}QComboBox,QDoubleSpinBox{background:#172936;border:1px solid "
            "#385062;border-radius:3px;padding:4px;selection-background-color:#197e79;}QCheckBox{spacing:6px;"
            "}"
            "QScrollArea{border:0;}QSplitter::handle{background:#182b39;}QStatusBar{color:#8ea8bb;}");
        connect(&sampleTimer_, &QTimer::timeout, this, &MainWindow::updateReading);
        sampleTimer_.start(33);
        connect(&workerTimer_, &QTimer::timeout, this, &MainWindow::tickWorker);
        workerTimer_.start(50);
        connect(&demoTimer_, &QTimer::timeout, this, [this] {
            session_.volumeTestBowl = false;

            for (int i = 0; i < 20; ++i) {
                auto s = simulate(sequence_++, clock_.nsecsElapsed() / 1000 + clockOffset_);

                if (!session_.captures.capture(s, s.timestampUs)) {
                    demoTimer_.stop();
                    demo_->setText("Start demo scan");
                    statusBar()->showMessage(QString::fromStdString(session_.captures.error));
                    break;
                }

                reading_ = s;
            }

            {
                QSignalBlocker bx(xyzControls_[0]), by(xyzControls_[1]), bz(xyzControls_[2]);
                xyzControls_[0]->setValue(reading_.position.x);
                xyzControls_[1]->setValue(reading_.position.y);
                xyzControls_[2]->setValue(reading_.position.z);
            }

            changed(false);
        });

        updateReading();
        updateStatus();
        statusBar()->showMessage("Simulation ready. Capture measured points after hardware integration.");
    }

    MainWindow::~MainWindow() {
        if (cancel_)
            cancel_->store(true);

        if (worker_.valid())
            worker_.wait();
    }

    void MainWindow::updateReading() {
        auto now = clock_.nsecsElapsed() / 1000 + clockOffset_;
        Snapshot s;
        s.timestampUs = now;
        s.sequence = sequence_;
        int mode = source_->currentIndex();

        if (mode == 0) {
            s.position = {xyzControls_[0]->value(), xyzControls_[1]->value(), xyzControls_[2]->value()};
            s.source = "SIMULATED XYZ";
        } else if (mode == 1) {
            s.hasAngles = true;
            s.source = "SIMULATED ANGLES";

            for (int i = 0; i < 5; ++i)
                s.counts[i] = (int(std::lround(jointControls_[i]->value() * 16384.0 / 360)) + 16384) % 16384;
            s.position = session_.geometry.tip(s.counts);
        } else if (mode == 2) {
            s.valid = false;
            s.source = "SPI NOT CONFIGURED";
        } else if (!replay_.empty()) {
            s = replay_[std::min(replayIndex_, replay_.size() - 1)];
            s.timestampUs = now;
            s.intervalUs = 0;
            s.source = "REPLAY / " + s.source;
        } else {
            s.valid = false;
            s.source = "REPLAY — no loaded records";
        }

        const bool moved = norm(s.position - reading_.position) > 1e-9 || s.valid != reading_.valid;
        reading_ = s;
        sourceStatus_->setText(QString::fromStdString(s.source) +
            (mode == 1 ? "\n" + QString::fromStdString(session_.geometry.name) : QString()));
        position_->setText(QString("X %1    Y %2    Z %3 mm")
                .arg(s.position.x, 0, 'f', 2)
                .arg(s.position.y, 0, 'f', 2)
                .arg(s.position.z, 0, 'f', 2));
        capture_->setEnabled(s.valid && session_.captures.points.size() < 100000);

        if (moved)
            refreshView();
    }

    void MainWindow::capture() {
        if (session_.captures.capture(reading_, clock_.nsecsElapsed() / 1000 + clockOffset_)) {
            session_.volumeTestBowl = false;
            ++sequence_;
            changed(false);
        } else
            statusBar()->showMessage(QString::fromStdString(session_.captures.error), 5000);
    }

    void MainWindow::changed(bool invalidateMesh, bool preserveWall) {
        revision_.change();

        if (invalidateMesh) {
            generation_.change();

            if (cancel_)
                cancel_->store(true);

            if (preserveWall && !mesh_.faces.empty()) {
                mesh_.capFaces.clear();
                mesh_.volumeValid = false;
                mesh_.volumeMm3 = 0;
                mesh_.capStatus = "Closure settings changed; showing the previous wall while updating.";
            } else {
                mesh_ = {};
                meshRevision_ = 0;
                meshPointCount_ = 0;
            }

            reconstructionFailure_.clear();
        }

        pending_ = !session_.captures.points.empty();
        dirty_ = true;
        refreshView();
        updateStatus();
    }

    void MainWindow::refreshView() {
        viewer_->setScene(session_.captures.points, mesh_, reading_.position);
    }

    void MainWindow::tickWorker() {
        if (worker_.valid() && worker_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                auto result = worker_.get();

                if (!result.cancelled && generation_.accepts(jobGeneration_) &&
                    jobRevision_ >= meshRevision_) {
                    if (result.faces.empty()) {
                        reconstructionFailure_ = result.limited
                            ? "Reconstruction reached its work limit without a surface."
                            : "Reconstruction produced no surface for the latest captures.";

                        if (!result.capStatus.empty())
                            reconstructionFailure_ += " " + QString::fromStdString(result.capStatus);
                    } else {
                        mesh_ = std::move(result);
                        meshRevision_ = jobRevision_;
                        meshPointCount_ = jobPointCount_;
                        reconstructionFailure_.clear();
                        refreshView();
                    }
                }
            } catch (const std::exception &e) {
                statusBar()->showMessage(QString("Reconstruction failed: ") + e.what());
            }

            updateStatus();
        }

        if (pending_ && !worker_.valid() && clock_.elapsed() - lastJobMs_ >= 1000) {
            std::vector<Vec3> points;
            points.reserve(session_.captures.points.size());

            for (const auto &s : session_.captures.points)
                points.push_back(s.position);
            auto settings = session_.settings;
            bool volumeTest = session_.volumeTestBowl;
            cancel_ = std::make_shared<std::atomic_bool>(false);
            auto cancel = cancel_;
            jobRevision_ = revision_.token();
            jobGeneration_ = generation_.token();
            jobPointCount_ = points.size();
            pending_ = false;
            lastJobMs_ = clock_.elapsed();
            worker_ =
                std::async(std::launch::async, [points = std::move(points), settings, cancel, volumeTest] {
                    if (volumeTest) {
                        auto mesh = makeCapTestBowl();

                        if (settings.flatCap && settings.curvedCap)
                            return closeCurvedOpening(mesh, settings.capOpening, [cancel] {
                                return cancel->load();
                            });

                        if (settings.flatCap)
                            return closeFlatOpening(
                                mesh, settings.capOpening, settings.capTolerance, [cancel] {
                                    return cancel->load();
                                });

                        return mesh;
                    }

                    return reconstruct(points, settings, [cancel] {
                        return cancel->load();
                    });
                });

            updateStatus();
        }
    }

    void MainWindow::updateStatus() {
        pointCount_->setText(QString("%1 / 100,000 raw captures").arg(session_.captures.points.size()));

        if (session_.volumeTestBowl)
            pointCount_->setText(
                QString("%1 synthetic bowl fixture points").arg(session_.captures.points.size()));
        meshStatus_->setProperty("triangleCount", int(mesh_.faces.size()));
        QString text = meshRevision_ ? QString("%1 triangles · revision %2\nSurface uses %3 / %4 captures%5")
                                            .arg(mesh_.faces.size())
                                            .arg(meshRevision_)
                                            .arg(meshPointCount_)
                                            .arg(session_.captures.points.size())
                                            .arg(mesh_.limited ? " · work limit reached" : "")
                                        : "Waiting for enough neighboring points.";

        if (pending_ || worker_.valid())
            text += "\nUpdating surface…";

        if (!reconstructionFailure_.isEmpty()) {
            text += "\n" + reconstructionFailure_;

            if (!mesh_.faces.empty())
                text += "\nShowing the previous surface; latest captures are not included.";
        }

        if (!session_.settings.flatCap && !mesh_.capStatus.empty())
            text += "\n" + QString::fromStdString(mesh_.capStatus);

        if (session_.settings.flatCap && meshRevision_) {
            text += QString("\n%1 openings · %2 cap triangles\n%3")
                        .arg(mesh_.openings)
                        .arg(mesh_.capFaces.size())
                        .arg(QString::fromStdString(mesh_.capStatus));

            if (mesh_.volumeValid && revision_.accepts(meshRevision_))
                text += QString("\nEstimated volume: %1 mm³ / %2 mL")
                            .arg(mesh_.volumeMm3, 0, 'f', 3)
                            .arg(mesh_.volumeMm3 / 1000.0, 0, 'f', 6);
            else if (mesh_.volumeValid)
                text += "\nVolume updating — previous surface is displayed.";
            else
                text += "\nVolume unavailable.";
        }

        meshStatus_->setText(text);
    }

    bool MainWindow::confirmDiscard() {
        return !dirty_ ||
            QMessageBox::question(this, "Unsaved captures", "Discard changes to this session?",
                QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Discard;
    }

    void MainWindow::newSession() {
        if (!confirmDiscard())
            return;
        demoTimer_.stop();
        demo_->setText("Start demo scan");
        session_.captures.points.clear();
        session_.volumeTestBowl = false;
        session_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        replay_.clear();
        replayIndex_ = 0;
        sequence_ = 0;
        savePath_.clear();
        changed();
        dirty_ = false;
        updateReading();
        viewer_->fit();
    }

    bool MainWindow::save() {
        QString path = savePath_;

        if (path.isEmpty())
            path = QFileDialog::getSaveFileName(
                this, "Save acquisition session", {}, "MedicalWand JSON (*.json)");

        if (path.isEmpty())
            return false;
        QString message;

        if (!saveSession(path, session_, message)) {
            error(message);
            return false;
        }

        savePath_ = path;
        dirty_ = false;
        statusBar()->showMessage("Session saved", 4000);
        return true;
    }

    void MainWindow::load() {
        QString path =
            QFileDialog::getOpenFileName(this, "Open acquisition session", {}, "MedicalWand JSON (*.json)");

        if (path.isEmpty())
            return;
        Session loaded;
        QString message;

        if (!loadSession(path, loaded, message)) {
            error(message);
            return;
        }

        if (!confirmDiscard())
            return;
        demoTimer_.stop();
        demo_->setText("Start demo scan");
        session_ = std::move(loaded);
        replay_ = session_.captures.points;
        replayIndex_ = 0;
        std::int64_t newest = 0;

        for (const auto &s : replay_)
            newest = std::max(newest, s.timestampUs);
        clockOffset_ = std::max(std::int64_t(0), newest - clock_.nsecsElapsed() / 1000 + 1);
        sequence_ = 0;

        for (const auto &s : replay_)
            sequence_ = std::max(sequence_, s.sequence + 1);
        {
            QSignalBlocker a(merge_), b(edge_), c(flatCap_), d(capTolerance_), e(capOpening_),
                curvedBlock(curvedCap_), f(reconstructionMode_);
            merge_->setValue(session_.settings.mergeDistance);
            edge_->setValue(session_.settings.maxEdge);
            flatCap_->setChecked(session_.settings.flatCap);
            curvedCap_->setChecked(session_.settings.curvedCap);
            curvedCap_->setEnabled(session_.settings.flatCap);
            capTolerance_->setValue(session_.settings.capTolerance);
            capOpening_->setValue(session_.settings.capOpening + 1);
            reconstructionMode_->setCurrentIndex(session_.settings.radialCavity ? 0 : 1);
            capTolerance_->setEnabled(session_.settings.flatCap && !session_.settings.curvedCap);
            capOpening_->setEnabled(session_.settings.flatCap);
        }

        source_->setCurrentIndex(3);
        savePath_ = path;
        changed();
        dirty_ = false;
        updateReading();
        viewer_->fit();
    }

    void MainWindow::replayNext() {
        if (source_->currentIndex() == 3 && !replay_.empty()) {
            replayIndex_ = (replayIndex_ + 1) % replay_.size();
            updateReading();
        }
    }

    void MainWindow::error(const QString &message) {
        QMessageBox::warning(this, "MedicalWand", message);
    }

    void MainWindow::closeEvent(QCloseEvent *event) {
        if (confirmDiscard())
            event->accept();
        else
            event->ignore();
    }

    void MainWindow::keyPressEvent(QKeyEvent *event) {
        // Child controls handle their normal keys first; only unclaimed Space reaches here.

        if (event->key() == Qt::Key_Space && !event->isAutoRepeat() && capture_->isEnabled()) {
            capture();
            event->accept();
        } else
            QMainWindow::keyPressEvent(event);
    }
} // namespace mw
