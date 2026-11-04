#pragma once
#include "session_io.h"
#include "viewer.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <atomic>
#include <future>

namespace mw {
    class MainWindow : public QMainWindow {
        Q_OBJECT

        public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow() override;

        protected:
        void closeEvent(QCloseEvent *) override;
        void keyPressEvent(QKeyEvent *) override;

        private:
        void updateReading();
        void capture();
        void changed(bool invalidateMesh = true);
        void tickWorker();
        void updateStatus();
        void refreshView();
        void newSession();
        bool confirmDiscard();
        bool save();
        void load();
        void replayNext();
        void error(const QString &);
        Session session_;
        Mesh mesh_;
        Snapshot reading_;
        Revision revision_;
        Revision generation_;
        Viewer *viewer_ = nullptr;
        QLabel *position_ = nullptr, *pointCount_ = nullptr, *meshStatus_ = nullptr, *sourceStatus_ = nullptr,
                *graphics_ = nullptr;
        QPushButton *capture_ = nullptr, *demo_ = nullptr;
        QComboBox *source_ = nullptr;
        std::array<QDoubleSpinBox *, 5> jointControls_{};
        std::array<QDoubleSpinBox *, 3> xyzControls_{};
        QDoubleSpinBox *merge_ = nullptr, *edge_ = nullptr;
        QCheckBox *flatCap_ = nullptr;
        QDoubleSpinBox *capTolerance_ = nullptr;
        QSpinBox *capOpening_ = nullptr;
        QCheckBox *pointLayer_ = nullptr, *surfaceLayer_ = nullptr;
        QTimer sampleTimer_, workerTimer_, demoTimer_;
        QElapsedTimer clock_;
        std::int64_t clockOffset_ = 0, lastJobMs_ = -1000;
        std::uint64_t sequence_ = 0, jobRevision_ = 0, meshRevision_ = 0;
        std::uint64_t jobGeneration_ = 0;
        size_t jobPointCount_ = 0, meshPointCount_ = 0;
        bool pending_ = false, dirty_ = false;
        std::future<Mesh> worker_;
        std::shared_ptr<std::atomic_bool> cancel_;
        std::vector<Snapshot> replay_;
        size_t replayIndex_ = 0;
        QString savePath_;
    };
} // namespace mw
