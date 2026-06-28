#include "mainwindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QMessageBox>
#include <QCloseEvent>
#include <QApplication>
#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QtConcurrent>

namespace chess {

// Pretty-print a piece for the captures list / dialogs.
static QString pieceName(Piece p)
{
    switch (p) {
    case Piece::RK: return QStringLiteral("帥"); case Piece::BK: return QStringLiteral("將");
    case Piece::RA: return QStringLiteral("仕"); case Piece::BA: return QStringLiteral("士");
    case Piece::RB: return QStringLiteral("相"); case Piece::BB: return QStringLiteral("象");
    case Piece::RN: case Piece::BN: return QStringLiteral("馬");
    case Piece::RR: case Piece::BR: return QStringLiteral("車");
    case Piece::RC: return QStringLiteral("炮"); case Piece::BC: return QStringLiteral("砲");
    case Piece::RP: return QStringLiteral("兵"); case Piece::BP: return QStringLiteral("卒");
    default: return QStringLiteral("?");
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("中国象棋"));

    // ---- Central layout: board on the left, side panel on the right ----
    auto *central = new QWidget(this);
    auto *outer = new QHBoxLayout(central);
    outer->setContentsMargins(6, 6, 6, 6);

    m_view = new BoardView(central);
    m_view->setBoard(&m_board);
    outer->addWidget(m_view, 1);

    auto *side = new QWidget(central);
    auto *sideL = new QVBoxLayout(side);
    sideL->setContentsMargins(0, 0, 0, 0);
    side->setFixedWidth(280);

    m_status = new QLabel(side);
    QFont sf = m_status->font(); sf.setPointSize(11); sf.setBold(true);
    m_status->setFont(sf);
    m_status->setWordWrap(true);
    sideL->addWidget(m_status);

    m_log = new QTextEdit(side);
    m_log->setReadOnly(true);
    m_log->setPlaceholderText(QStringLiteral("对局记录与聊天信息将显示在这里…"));
    sideL->addWidget(m_log, 1);

    auto *capLabel = new QLabel(QStringLiteral("被吃的棋子："), side);
    sideL->addWidget(capLabel);
    m_captures = new QListWidget(side);
    m_captures->setMaximumHeight(90);
    sideL->addWidget(m_captures);

    auto *chatRow = new QHBoxLayout;
    m_chatEdit = new QLineEdit(side);
    m_chatEdit->setPlaceholderText(QStringLiteral("输入聊天消息…"));
    m_chatSend = new QPushButton(QStringLiteral("发送"), side);
    chatRow->addWidget(m_chatEdit, 1);
    chatRow->addWidget(m_chatSend);
    sideL->addLayout(chatRow);

    outer->addWidget(side);
    setCentralWidget(central);

    // ---- Menu / toolbar ----
    auto *mb = menuBar();
    auto *gameMenu = mb->addMenu(QStringLiteral("游戏(&G)"));
    auto *actNew = gameMenu->addAction(QStringLiteral("新游戏…(&N)"));
    gameMenu->addSeparator();
    auto *actUndo = gameMenu->addAction(QStringLiteral("悔棋(&U)"));
    auto *actResign = gameMenu->addAction(QStringLiteral("认输(&R)"));
    gameMenu->addSeparator();
    auto *actQuit = gameMenu->addAction(QStringLiteral("退出(&Q)"));

    auto *tb = addToolBar(QStringLiteral("主工具栏"));
    tb->setMovable(false);
    tb->addAction(actNew);
    tb->addAction(actUndo);
    tb->addAction(actResign);

    connect(actNew, &QAction::triggered, this, &MainWindow::onNewGame);
    connect(actUndo, &QAction::triggered, this, &MainWindow::onUndo);
    connect(actResign, &QAction::triggered, this, &MainWindow::onResign);
    connect(actQuit, &QAction::triggered, qApp, &QApplication::quit);

    connect(m_view, &BoardView::moveRequested, this, &MainWindow::onMoveRequested);
    connect(m_chatSend, &QPushButton::clicked, this, &MainWindow::onSendChat);
    connect(m_chatEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendChat);

    // AI runs on a thread pool; we collect the result via a watcher.
    connect(&m_aiWatcher, &QFutureWatcher<Move>::finished, this, &MainWindow::onAiFinished);

    // Networking.
    m_net = new NetworkManager(this);
    connect(m_net, &NetworkManager::connected, this, &MainWindow::onNetConnected);
    connect(m_net, &NetworkManager::disconnected, this, &MainWindow::onNetDisconnected);
    connect(m_net, &NetworkManager::errorOccurred, this, &MainWindow::onNetError);
    connect(m_net, &NetworkManager::moveReceived, this, &MainWindow::onNetMoveReceived);
    connect(m_net, &NetworkManager::chatReceived, this, &MainWindow::onNetChatReceived);
    connect(m_net, &NetworkManager::resignReceived, this, &MainWindow::onNetResign);
    connect(m_net, &NetworkManager::rematchReceived, this, &MainWindow::onNetRematch);
    connect(m_net, &NetworkManager::helloReceived, this, [this](const QString &name){
        m_peerName = name;
        appendLog(QStringLiteral("对方昵称：%1").arg(name));
    });

    resize(900, 720);
    statusBar()->showMessage(QStringLiteral("Ready. Select New Game from the Game menu."));
}

MainWindow::~MainWindow()
{
    if (m_aiWatcher.isRunning())
        m_aiWatcher.waitForFinished();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (m_net) m_net->disconnect();
    e->accept();
}

// ---- new game / mode selection ----

void MainWindow::onNewGame()
{
    GameModeDialog dlg(this);
    if (dlg.exec() == QDialog::Rejected) {
        if (m_mode == Mode::None)
            m_status->setText(QStringLiteral("请点击“新游戏”开始。"));
        return;
    }
    startGame(dlg);
}

void MainWindow::startGame(const GameModeDialog &dlg)
{
    if (m_net) m_net->disconnect();
    m_peerName.clear();
    m_gameOver = false;
    m_aiBusy = false;
    ++m_gameGeneration;
    m_myName = dlg.playerName().isEmpty() ? QStringLiteral("玩家") : dlg.playerName();

    switch (dlg.selectedMode()) {
    case GameModeDialog::VsAI:
        m_mode = Mode::VsAI;
        m_humanSide = dlg.aiHumanSide();
        m_diff = dlg.aiDifficulty();
        m_ai.setDifficulty(m_diff);
        break;
    case GameModeDialog::HostNet: {
        m_mode = Mode::HostNet;
        if (!m_net->host(dlg.hostPort())) return;
        appendLog(QStringLiteral("正在监听端口 %1，等待好友加入…").arg(dlg.hostPort()));
        break;
    }
    case GameModeDialog::JoinNet: {
        m_mode = Mode::JoinNet;
        appendLog(QStringLiteral("正在连接 %1:%2 …").arg(dlg.joinHost()).arg(dlg.joinPort()));
        m_net->join(dlg.joinHost(), dlg.joinPort());
        break;
    }
    case GameModeDialog::Hotseat:
        m_mode = Mode::Hotseat;
        m_humanSide = Side::Red;
        break;
    default:
        m_mode = Mode::None;
        return;
    }

    resetBoardAndUi();
    m_history.clear();
    maybeFlipView();
    updateStatus();
    refreshView();

    // VsAI: if human is Black, AI (Red) opens.
    if (m_mode == Mode::VsAI && m_humanSide != m_board.sideToMove())
        startAiTurn();
}

void MainWindow::resetBoardAndUi()
{
    m_board.reset();
    m_view->clearSelection();
    m_view->clearHints();
    m_view->clearLastMove();
    m_log->clear();
    m_captures->clear();
    m_view->setBoard(&m_board);
}

void MainWindow::maybeFlipView()
{
    Side bottom = Side::Red;
    switch (m_mode) {
    case Mode::VsAI:    bottom = m_humanSide; break;
    case Mode::HostNet: bottom = Side::Red;    break;
    case Mode::JoinNet: bottom = Side::Black;  break;
    case Mode::Hotseat: bottom = m_board.sideToMove(); break; // flip each turn
    default: break;
    }
    m_view->setLocalSide(bottom);
}

bool MainWindow::isLocalTurn() const
{
    Side toMove = m_board.sideToMove();
    switch (m_mode) {
    case Mode::VsAI:    return toMove == m_humanSide;
    case Mode::HostNet: return toMove == Side::Red;
    case Mode::JoinNet: return toMove == Side::Black;
    case Mode::Hotseat: return true;
    default: return false;
    }
}

void MainWindow::refreshView()
{
    m_view->setBoard(&m_board);
    m_view->update();
}

void MainWindow::updateStatus()
{
    QString modeText;
    switch (m_mode) {
    case Mode::VsAI:    modeText = QStringLiteral("人机对战"); break;
    case Mode::HostNet: modeText = QStringLiteral("联机（主机·红方）"); break;
    case Mode::JoinNet: modeText = QStringLiteral("联机（加入·黑方）"); break;
    case Mode::Hotseat: modeText = QStringLiteral("本地双人"); break;
    default: modeText = QStringLiteral("未开始"); break;
    }
    Side s = m_board.sideToMove();
    QString turn = (s == Side::Red) ? QStringLiteral("红方") : QStringLiteral("黑方");
    QString suffix;
    if (m_board.inCheck(s)) suffix = QStringLiteral("  ⚑ 将军！");
    if (m_gameOver) suffix = QStringLiteral("  · 对局结束");
    if (m_aiBusy) suffix += QStringLiteral("  (AI 思考中…)");

    m_status->setText(QStringLiteral("%1   轮到 %2 行棋%3").arg(modeText, turn, suffix));
    setWindowTitle(QStringLiteral("中国象棋 — %1").arg(modeText));
}

void MainWindow::appendLog(const QString &s)
{
    m_log->append(s);
}

void MainWindow::recordCapture(Piece captured, Side mover)
{
    if (captured == Piece::None) return;
    QString label = (mover == Side::Red) ? QStringLiteral("红方吃：") : QStringLiteral("黑方吃：");
    m_captures->addItem(label + pieceName(captured));
}

void MainWindow::handleGameOver()
{
    if (m_gameOver) return;
    Side toMove = m_board.sideToMove();
    if (m_board.isCheckmate()) {
        m_gameOver = true;
        Side winner = (toMove == Side::Red) ? Side::Black : Side::Red;
        QString w = (winner == Side::Red) ? QStringLiteral("红方") : QStringLiteral("黑方");
        appendLog(QStringLiteral("将杀！%1 获胜。").arg(w));
        QMessageBox::information(this, QStringLiteral("对局结束"),
            QStringLiteral("%1 将杀对手，获得胜利！").arg(w));
        updateStatus();
        return;
    }
    if (m_board.isStalemate()) {
        m_gameOver = true;
        appendLog(QStringLiteral("无棋可走，和棋。"));
        QMessageBox::information(this, QStringLiteral("对局结束"),
            QStringLiteral("无棋可走，本局和棋。"));
        updateStatus();
    }
}

// ---- human move attempts ----

void MainWindow::onMoveRequested(int fr, int fc, int tr, int tc)
{
    if (m_gameOver) return;
    if (m_aiBusy) return;
    if (!isLocalTurn()) {
        appendLog(QStringLiteral("现在不是你的回合。"));
        m_view->clearSelection();
        m_view->clearHints();
        return;
    }

    Move m;
    if (!m_board.tryMove(fr, fc, tr, tc, m)) {
        // Illegal: re-select if the target is one of our pieces, else deselect.
        Piece dest = m_board.at(tr, tc);
        if (dest != Piece::None && sideOf(dest) == m_board.sideToMove()) {
            m_view->setSelected(tr, tc);
            std::vector<Move> all = m_board.generateMoves();
            std::vector<Move> mine;
            for (auto &mm : all) if (mm.fr == tr && mm.fc == tc) mine.push_back(mm);
            m_view->setHints(mine);
        } else {
            m_view->clearSelection();
            m_view->clearHints();
        }
        return;
    }

    // tryMove applied + toggled side.
    m_history.push_back(m);
    Side mover = (m_board.sideToMove() == Side::Red) ? Side::Black : Side::Red;
    recordCapture(m.captured, mover);
    m_view->setLastMove(fr, fc, tr, tc);
    m_view->clearSelection();
    m_view->clearHints();
    appendLog(QStringLiteral("%1：%2,%3 → %4,%5")
                  .arg(mover == Side::Red ? QStringLiteral("红") : QStringLiteral("黑"))
                  .arg(fr).arg(fc).arg(tr).arg(tc));

    if ((m_mode == Mode::HostNet || m_mode == Mode::JoinNet) && m_net->isConnected())
        m_net->sendMove(fr, fc, tr, tc);

    maybeFlipView();
    refreshView();
    updateStatus();
    handleGameOver();

    if (!m_gameOver && m_mode == Mode::VsAI && m_board.sideToMove() != m_humanSide)
        startAiTurn();
}

// ---- AI ----

void MainWindow::startAiTurn()
{
    if (m_aiBusy || m_gameOver) return;
    m_aiBusy = true;
    updateStatus();
    // Snapshot the AI and board so the worker thread can't race with input.
    AI ai = m_ai;
    Board snapshot = m_board;
    unsigned gen = m_gameGeneration;
    // chooseMove mutates its internal RNG (member of `ai`) but not the board.
    QFuture<Move> fut = QtConcurrent::run([ai, snapshot, gen]() mutable -> Move {
        Q_UNUSED(gen);
        return ai.chooseMove(snapshot);
    });
    m_aiWatcher.setFuture(fut);
}

void MainWindow::onAiFinished()
{
    Move m = m_aiWatcher.result();
    m_aiBusy = false;
    if (m_gameOver) { updateStatus(); return; }
    if (m.fr == m.tr && m.fc == m.tc) {
        // AI had no moves — game should already be over; refresh just in case.
        handleGameOver();
        updateStatus();
        return;
    }

    // Validate the AI's move is legal before applying.
    Move applied;
    if (!m_board.tryMove(m.fr, m.fc, m.tr, m.tc, applied)) {
        updateStatus();
        return;
    }
    m_history.push_back(applied);
    Side mover = (m_board.sideToMove() == Side::Red) ? Side::Black : Side::Red;
    recordCapture(applied.captured, mover);
    m_view->setLastMove(m.fr, m.fc, m.tr, m.tc);
    appendLog(QStringLiteral("%1（AI）：%2,%3 → %4,%5")
                  .arg(mover == Side::Red ? QStringLiteral("红") : QStringLiteral("黑"))
                  .arg((int)m.fr).arg((int)m.fc).arg((int)m.tr).arg((int)m.tc));

    maybeFlipView();
    refreshView();
    updateStatus();
    handleGameOver();
}

// ---- network callbacks ----

void MainWindow::onNetConnected()
{
    appendLog(QStringLiteral("已与对手连接！"));
    if (!m_myName.isEmpty()) m_net->sendHello(m_myName);
    updateStatus();
}

void MainWindow::onNetDisconnected()
{
    appendLog(QStringLiteral("与对手的连接已断开。"));
    m_aiBusy = false;
    if (!m_gameOver) {
        m_gameOver = true;
        QMessageBox::information(this, QStringLiteral("联机中断"),
            QStringLiteral("与对手的连接已断开，本局结束。"));
    }
    updateStatus();
}

void MainWindow::onNetError(const QString &msg)
{
    appendLog(QStringLiteral("网络错误：%1").arg(msg));
    statusBar()->showMessage(msg, 5000);
}

void MainWindow::onNetMoveReceived(int fr, int fc, int tr, int tc)
{
    if (m_gameOver) return;
    if (!Board::inBoard(fr, fc) || !Board::inBoard(tr, tc)) {
        appendLog(QStringLiteral("收到非法坐标，已忽略。"));
        return;
    }
    // It's the opponent's turn; apply their move.
    Move m;
    if (!m_board.tryMove(fr, fc, tr, tc, m)) {
        appendLog(QStringLiteral("收到非法走子，已忽略。"));
        return;
    }
    m_history.push_back(m);
    Side mover = (m_board.sideToMove() == Side::Red) ? Side::Black : Side::Red;
    recordCapture(m.captured, mover);
    m_view->setLastMove(fr, fc, tr, tc);
    appendLog(QStringLiteral("%1：%2,%3 → %4,%5")
                  .arg(mover == Side::Red ? QStringLiteral("红") : QStringLiteral("黑"))
                  .arg(fr).arg(fc).arg(tr).arg(tc));
    maybeFlipView();
    refreshView();
    updateStatus();
    handleGameOver();
}

void MainWindow::onNetChatReceived(const QString &text)
{
    appendLog(QStringLiteral("[对手] %1").arg(text));
}

void MainWindow::onNetResign()
{
    appendLog(QStringLiteral("对手认输，你赢了！"));
    m_gameOver = true;
    QMessageBox::information(this, QStringLiteral("对局结束"),
        QStringLiteral("对手认输，恭喜你获胜！"));
    updateStatus();
}

void MainWindow::onNetRematch()
{
    auto rc = QMessageBox::question(this, QStringLiteral("再来一局"),
        QStringLiteral("对手请求再来一局，是否同意？"));
    if (rc != QMessageBox::Yes) return;
    m_gameOver = false;
    ++m_gameGeneration;
    m_history.clear();
    resetBoardAndUi();
    maybeFlipView();
    updateStatus();
    refreshView();
}

void MainWindow::onSendChat()
{
    QString text = m_chatEdit->text().trimmed();
    if (text.isEmpty()) return;
    if (m_mode == Mode::HostNet || m_mode == Mode::JoinNet) {
        if (m_net->isConnected()) m_net->sendChat(text);
    }
    appendLog(QStringLiteral("[我] %1").arg(text));
    m_chatEdit->clear();
}

void MainWindow::onResign()
{
    if (m_gameOver || m_mode == Mode::None) return;
    auto rc = QMessageBox::question(this, QStringLiteral("认输"),
        QStringLiteral("确定认输吗？"));
    if (rc != QMessageBox::Yes) return;
    m_gameOver = true;
    appendLog(QStringLiteral("你认输了。"));
    if ((m_mode == Mode::HostNet || m_mode == Mode::JoinNet) && m_net->isConnected())
        m_net->sendResign();
    updateStatus();
}

void MainWindow::onUndo()
{
    if (m_gameOver) return;
    if (m_mode == Mode::HostNet || m_mode == Mode::JoinNet) {
        QMessageBox::information(this, QStringLiteral("悔棋"),
            QStringLiteral("联机对战暂不支持悔棋。"));
        return;
    }
    // In VsAI undo two plies (yours + AI's); in hotseat undo one ply.
    int plies = (m_mode == Mode::VsAI) ? 2 : 1;
    if ((int)m_history.size() < plies) {
        statusBar()->showMessage(QStringLiteral("没有可悔的棋。"), 3000);
        return;
    }
    for (int i = 0; i < plies; ++i) {
        Move m = m_history.back();
        m_history.pop_back();
        m_board.toggleSide();
        m_board.unmakeMove(m);
        // reflect the undone capture in the list
        if (m.captured != Piece::None && m_captures->count() > 0) {
            delete m_captures->takeItem(m_captures->count() - 1);
        }
    }
    m_view->clearSelection();
    m_view->clearHints();
    m_view->clearLastMove();
    appendLog(QStringLiteral("已悔棋。"));
    maybeFlipView();
    refreshView();
    updateStatus();
}

} // namespace chess
