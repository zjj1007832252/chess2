#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFutureWatcher>
#include "board.h"
#include "ai.h"
#include "boardview.h"
#include "networkmanager.h"
#include "gamemodedialog.h"

class QLabel;
class QListWidget;
class QLineEdit;
class QPushButton;
class QTextEdit;

// Make Move usable across thread boundaries (queued signals / QFuture).
Q_DECLARE_METATYPE(chess::Move)

namespace chess {

// Top-level controller. Owns the Board, AI, view, and (optionally) a network
// link. Game modes:
//   - VsAI:    human vs computer. The AI runs on a QtConcurrent worker so the
//              UI stays responsive while it thinks.
//   - HostNet: this machine is the TCP server and plays Red (moves first).
//   - JoinNet: this machine connects to a friend and plays Black.
//   - Hotseat: two humans share the keyboard; the view flips each turn.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onNewGame();
    void onMoveRequested(int fr, int fc, int tr, int tc);
    void onAiFinished();
    void onNetConnected();
    void onNetDisconnected();
    void onNetError(const QString &msg);
    void onNetMoveReceived(int fr, int fc, int tr, int tc);
    void onNetChatReceived(const QString &text);
    void onNetResign();
    void onNetRematch();
    void onSendChat();
    void onResign();
    void onUndo();

private:
    enum class Mode { None, VsAI, HostNet, JoinNet, Hotseat };

    void startGame(const GameModeDialog &dlg);
    void resetBoardAndUi();
    void refreshView();
    void updateStatus();
    void appendLog(const QString &s);
    void recordCapture(Piece captured, Side mover);
    bool isLocalTurn() const;
    void maybeFlipView();
    void handleGameOver();
    void startAiTurn();

    Board m_board;
    BoardView *m_view = nullptr;
    QLabel *m_status = nullptr;
    QTextEdit *m_log = nullptr;
    QListWidget *m_captures = nullptr;

    QLineEdit *m_chatEdit = nullptr;
    QPushButton *m_chatSend = nullptr;

    Mode m_mode = Mode::None;
    Side m_humanSide = Side::Red;     // for VsAI: which side is the human
    Difficulty m_diff = Difficulty::Normal;

    AI m_ai;
    QFutureWatcher<Move> m_aiWatcher;
    bool m_aiBusy = false;

    // Move history for undo (only meaningful in non-network modes).
    std::vector<Move> m_history;

    NetworkManager *m_net = nullptr;
    QString m_peerName;
    QString m_myName;

    bool m_gameOver = false;
    unsigned m_gameGeneration = 0;
};

} // namespace chess
#endif // MAINWINDOW_H
