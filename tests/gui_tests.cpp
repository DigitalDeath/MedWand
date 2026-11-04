#include "main_window.h"
#include <QApplication>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
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

    auto capture = window.findChild<QPushButton *>("capture");
    auto count = window.findChild<QLabel *>("pointCount");
    auto undo = window.findChild<QPushButton *>("undo");
    auto demo = window.findChild<QPushButton *>("demo");
    auto viewer = window.findChild<mw::Viewer *>();
    auto source = window.findChild<QComboBox *>("source");
    auto cap = window.findChild<QCheckBox *>("flatCap");

    if (!cap) {
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

    if (status->text().contains("Estimated volume:")) {
        std::cerr << "Volume persisted after removing cap\n";
        return 1;
    }

    return 0;
}
