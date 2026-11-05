#include "main_window.h"
#include <QApplication>
#include <QMessageBox>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>

namespace mw {
    struct MainWindowTestAccess {
        static bool checkCapChange(MainWindow &window) {
            window.sampleTimer_.stop();
            window.workerTimer_.stop();
            window.session_.captures.points.resize(4);
            window.mesh_.vertices = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
            window.mesh_.faces = {{0, 1, 2}};
            window.meshRevision_ = window.revision_.token();
            window.meshPointCount_ = 4;
            window.flatCap_->setChecked(true);
            bool retained = window.mesh_.faces.size() == 1 && window.mesh_.capFaces.empty() &&
                !window.mesh_.volumeValid && !window.revision_.accepts(window.meshRevision_);
            window.curvedCap_->setChecked(true);
            retained = retained && window.mesh_.faces.size() == 1;
            window.flatCap_->setChecked(false);
            retained = retained && window.mesh_.faces.size() == 1;
            window.session_.captures.points.clear();
            window.curvedCap_->setChecked(false);
            window.changed();
            window.dirty_ = false;
            window.sampleTimer_.start();
            window.workerTimer_.start();
            return retained;
        }

        static bool checkSurfaceFailure(MainWindow &window) {
            window.sampleTimer_.stop();
            window.workerTimer_.stop();
            window.session_.settings.flatCap = true;
            window.mesh_ = {};
            window.mesh_.vertices = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
            window.mesh_.faces = {{0, 1, 2}};
            window.mesh_.volumeValid = true;
            window.mesh_.volumeMm3 = 1;
            window.meshRevision_ = window.revision_.token();
            window.meshPointCount_ = 3;
            window.session_.captures.points.resize(4);
            window.changed(false);
            window.jobRevision_ = window.revision_.token();
            window.jobGeneration_ = window.generation_.token();
            window.jobPointCount_ = 4;
            window.pending_ = false;
            std::promise<Mesh> completed;
            window.worker_ = completed.get_future();
            Mesh failed;
            failed.limited = true;
            completed.set_value(failed);
            window.tickWorker();

            if (window.mesh_.faces.size() != 1 || window.meshPointCount_ != 3 ||
                window.revision_.accepts(window.meshRevision_) ||
                !window.meshStatus_->text().contains("previous surface") ||
                !window.meshStatus_->text().contains("work limit") ||
                window.meshStatus_->text().contains("Estimated volume:"))
                return false;

            std::promise<Mesh> recovered;
            window.worker_ = recovered.get_future();
            recovered.set_value(window.mesh_);
            window.tickWorker();

            if (!window.revision_.accepts(window.meshRevision_) || window.meshPointCount_ != 4 ||
                window.meshStatus_->text().contains("previous surface") ||
                !window.meshStatus_->text().contains("Estimated volume:"))
                return false;

            std::promise<Mesh> obsolete;
            window.worker_ = obsolete.get_future();
            obsolete.set_value(window.mesh_);
            window.changed();
            window.pending_ = false;
            window.tickWorker();

            if (!window.mesh_.faces.empty() || !window.reconstructionFailure_.isEmpty())
                return false;

            window.session_.captures.points.clear();
            window.session_.settings.flatCap = false;
            window.changed();
            window.dirty_ = false;
            window.sampleTimer_.start();
            window.workerTimer_.start();
            return true;
        }
    };
} // namespace mw

int main(int argc, char **argv) {
    const bool noGraphics = argc > 1 && QString::fromLocal8Bit(argv[1]) == "--no-graphics";

    if (noGraphics)
        qputenv("QT_QPA_PLATFORM", "minimal");
    mw::configureGraphics();
    QApplication app(argc, argv);
    mw::MainWindow window;
    window.show();

    if (noGraphics) {
        QTest::qWait(2100);
        auto failure = window.findChild<QLabel *>("graphicsFailure");

        if (!failure || !failure->isVisible()) {
            std::cerr << "Missing visible context-failure explanation\n";
            return 1;
        }

        std::cout << "Context failure is visibly reported\n";
        return 0;
    }

    if (!QTest::qWaitForWindowExposed(&window, 5000)) {
        std::cerr << "Window not exposed\n";
        return 1;
    }

    if (!mw::MainWindowTestAccess::checkSurfaceFailure(window)) {
        std::cerr << "Failed reconstruction erased the last compatible surface or exposed stale volume\n";
        return 1;
    }

    if (!mw::MainWindowTestAccess::checkCapChange(window)) {
        std::cerr << "Changing cap checkboxes discarded the cavity wall\n";
        return 1;
    }

    auto capture = window.findChild<QPushButton *>("capture");
    auto count = window.findChild<QLabel *>("pointCount");
    auto undo = window.findChild<QPushButton *>("undo");
    auto demo = window.findChild<QPushButton *>("demo");
    auto viewer = window.findChild<mw::Viewer *>();
    auto source = window.findChild<QComboBox *>("source");
    auto cap = window.findChild<QCheckBox *>("flatCap");
    auto stlExport = window.findChild<QPushButton *>("exportStl");

    if (!stlExport || stlExport->isEnabled()) {
        std::cerr << "STL export must be disabled without a closed current surface\n";
        return 1;
    }

    auto curvedCap = window.findChild<QCheckBox *>("curvedCap");

    if (!cap || !curvedCap) {
        std::cerr << "Missing flat-cap control\n";
        return 1;
    }

    if (!capture || !count || !undo || !demo || !viewer || !source) {
        std::cerr << "Missing capture controls\n";
        return 1;
    }

    QTest::qWait(250);

    if (!viewer->graphicsOk()) {
        std::cerr << viewer->diagnostics().toStdString() << '\n';
        return 1;
    }

    // Qt painting or other GL users may leave depth writes disabled. A new
    // surface frame must clear stale depth regardless of the incoming state.
    mw::Mesh depthFixture;
    depthFixture.vertices = {{-20, -20, 0}, {20, -20, 0}, {0, 20, 0}};
    depthFixture.faces = {{0, 1, 2}};
    viewer->setScene({}, depthFixture, {});
    viewer->setLayers(false, true);
    viewer->standardView(1);
    viewer->fit();
    auto countSurfacePixels = [](const QImage &frame) {
        int count = 0;

        for (int y = 0; y < frame.height(); ++y) {
            for (int x = 0; x < frame.width(); ++x) {
                auto color = frame.pixelColor(x, y);

                if (color.green() > 60 && color.blue() > 60 && color.green() > color.red() * 1.7 &&
                    color.blue() > color.red() * 1.7)
                    ++count;
            }
        }

        return count;
    };

    if (countSurfacePixels(viewer->grabFramebuffer()) < 100) {
        std::cerr << "Surface pixel fixture is not visible before state regression\n";
        return 1;
    }

    viewer->makeCurrent();
    auto gl = viewer->context()->functions();
    GLboolean incomingDepthMask = GL_TRUE;
    gl->glGetBooleanv(GL_DEPTH_WRITEMASK, &incomingDepthMask);
    std::cout << "Depth write mask after Qt painting: " << int(incomingDepthMask) << '\n';
    gl->glDepthMask(GL_TRUE);
    gl->glClearDepthf(0);
    gl->glClear(GL_DEPTH_BUFFER_BIT);
    gl->glClearDepthf(1);
    gl->glDepthMask(GL_FALSE);
    viewer->doneCurrent();

    if (countSurfacePixels(viewer->grabFramebuffer()) < 100) {
        std::cerr << "Incoming GL depth state hides the surface permanently\n";
        return 1;
    }

    viewer->setLayers(true, true);
    viewer->standardView(0);
    mw::MainWindowTestAccess::checkSurfaceFailure(window);

    QTest::mouseClick(capture, Qt::LeftButton);

    if (!count->text().startsWith("1 /")) {
        std::cerr << "Capture did not update cloud\n";
        return 1;
    }

    undo->setFocus();
    QTest::keyClick(undo, Qt::Key_Space);

    if (!count->text().startsWith("0 /")) {
        std::cerr << "Undo failed\n";
        return 1;
    }

    viewer->setFocus();
    QTest::keyClick(viewer, Qt::Key_Space);

    if (!count->text().startsWith("1 /")) {
        std::cerr << "Viewport Space did not capture\n";
        return 1;
    }

    QTest::mouseClick(undo, Qt::LeftButton);
    auto numeric = window.findChild<QDoubleSpinBox *>();
    numeric->setFocus();
    QTest::keyClick(numeric, Qt::Key_Space);

    if (!count->text().startsWith("0 /")) {
        std::cerr << "Numeric editor Space unexpectedly captured\n";
        return 1;
    }

    source->setCurrentIndex(2);
    QTest::qWait(40);

    if (capture->isEnabled()) {
        std::cerr << "Unconfigured SPI capture enabled\n";
        return 1;
    }

    source->setCurrentIndex(0);
    QTest::qWait(40);
    QTest::mouseClick(demo, Qt::LeftButton);
    QTest::qWait(2600);
    auto liveStatus = window.findChild<QLabel *>("meshStatus");

    if (!liveStatus || liveStatus->property("triangleCount").toInt() <= 0) {
        std::cerr << "Surface must develop while scan runs\n";
        return 1;
    }

    QTest::mouseClick(demo, Qt::LeftButton);

    if (count->text().startsWith("0 /")) {
        std::cerr << "Demo did not capture\n";
        return 1;
    }

    // Wait for the last revision so the screenshot checks a real generated surface.
    QTest::qWait(1800);
    auto status = window.findChild<QLabel *>("meshStatus");

    if (!status || !status->text().contains("triangles")) {
        std::cerr << "No reconstructed result\n";
        return 1;
    }

    if (argc > 1) {
        if (argc > 2) {
            QTest::mouseClick(demo, Qt::LeftButton);
            QTest::qWait(QString::fromLocal8Bit(argv[2]).toInt());
            QTest::mouseClick(demo, Qt::LeftButton);
            QTest::qWait(1500);
        }

        window.grab().save(QString::fromLocal8Bit(argv[1]));
    }

    std::cout << viewer->diagnostics().toStdString() << '\n'
                << status->text().toStdString() << "\nGUI checks passed\n";
    // A destructive reset must invalidate displayed/queued worker results.
    auto fresh = window.findChild<QPushButton *>("newSession");

    if (!fresh)
        return 1;
    QTest::mouseClick(demo, Qt::LeftButton);
    QTest::qWait(1050);
    QTimer::singleShot(30, [] {
        auto dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());

        if (dialog)
            dialog->button(QMessageBox::Discard)->click();
    });

    QTest::mouseClick(fresh, Qt::LeftButton);
    QTest::qWait(1500);

    if (!count->text().startsWith("0 /") || status->property("triangleCount").toInt() != 0) {
        std::cerr << "Old reconstruction reappeared after reset\n";
        return 1;
    }

    // Destructor safely cancels/joins the worker; no discard dialog needed.
    QTest::mouseClick(demo, Qt::LeftButton);
    QElapsedTimer scanClock;
    scanClock.start();

    bool toggledDuringScan = false;

    while (count->text().section(' ', 0, 0).toInt() < 6000 && scanClock.elapsed() < 30000) {
        QTest::qWait(50);

        if (!toggledDuringScan && count->text().section(' ', 0, 0).toInt() >= 3000 &&
            status->property("triangleCount").toInt() > 100) {
            auto pointLayer = window.findChild<QCheckBox *>("pointsLayer");
            pointLayer->setChecked(false);

            for (bool close : {true, false, true, false}) {
                cap->setChecked(close);
                QTest::qWait(60);

                if (status->property("triangleCount").toInt() == 0 ||
                    countSurfacePixels(viewer->grabFramebuffer()) < 100) {
                    std::cerr << "Live checkbox toggle removed the visible demo wall\n";
                    return 1;
                }
            }

            pointLayer->setChecked(true);
            toggledDuringScan = true;
        }
    }

    if (!toggledDuringScan) {
        std::cerr << "Live checkbox regression did not reach a surface\n";
        return 1;
    }

    QTest::mouseClick(demo, Qt::LeftButton);

    if (count->text().section(' ', 0, 0).toInt() < 6000) {
        std::cerr << "Regular demo did not reach test density\n";
        return 1;
    }

    cap->setChecked(true);
    QElapsedTimer volumeClock;
    volumeClock.start();

    while (!status->text().contains("Estimated volume:") && volumeClock.elapsed() < 6000)
        QTest::qWait(50);

    if (!status->text().contains("Estimated volume:")) {
        std::cerr << "Regular scan/stop/cap workflow failed: " << status->text().toStdString() << '\n';
        return 1;
    }

    if (!stlExport->isEnabled()) {
        std::cerr << "Closed current cavity did not enable STL export\n";
        return 1;
    }

    std::cout << "Regular captured scan capped: " << status->text().toStdString() << '\n';
    curvedCap->setChecked(true);
    volumeClock.restart();

    while ((!status->text().contains("Estimated volume:") || !status->text().contains("curved cap")) &&
        volumeClock.elapsed() < 6000)
        QTest::qWait(50);

    if (!status->text().contains("curved cap") || !status->text().contains("Estimated volume:")) {
        std::cerr << "Curved scan cap failed: " << status->text().toStdString() << '\n';
        return 1;
    }

    const auto previewPath = qEnvironmentVariable("MEDWAND_PREVIEW_PATH");

    if (!previewPath.isEmpty()) {
        viewer->setLayers(false, true);
        viewer->fit();
        viewer->standardView(0);
        QTest::qWait(200);

        if (!window.grab().save(previewPath))
            return 1;
        viewer->setLayers(true, true);
    }

    QTimer::singleShot(30, [] {
        auto dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());

        if (dialog)
            dialog->button(QMessageBox::Discard)->click();
    });

    auto bowl = window.findChild<QPushButton *>("testBowl");

    if (!bowl)
        return 1;
    bowl->click();
    QTest::qWait(2300);

    if (!status->text().contains("Estimated volume:")) {
        std::cerr << "Synthetic bowl volume not displayed: " << status->text().toStdString() << '\n';
        return 1;
    }

    cap->setChecked(false);
    QTest::qWait(1500);

    if (stlExport->isEnabled() || status->text().contains("Estimated volume:")) {
        std::cerr << "Volume persisted after removing cap\n";
        return 1;
    }

    return 0;
}
