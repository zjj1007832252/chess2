#include "gamemodedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QAbstractSocket>
#include <QNetworkInterface>
#include <cstring>

namespace chess {

GameModeDialog::GameModeDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("中国象棋 — 选择游戏模式"));
    setMinimumWidth(420);

    QVBoxLayout *root = new QVBoxLayout(this);

    QHBoxLayout *cols = new QHBoxLayout;
    root->addLayout(cols);

    m_list = new QListWidget(this);
    m_list->addItem(QStringLiteral("与人机对战"));
    m_list->addItem(QStringLiteral("创建联机房间（主机）"));
    m_list->addItem(QStringLiteral("加入联机房间"));
    m_list->addItem(QStringLiteral("本地双人对战"));
    m_list->setCurrentRow(0);
    cols->addWidget(m_list);

    m_stack = new QStackedWidget(this);
    cols->addWidget(m_stack, 1);

    // --- Panel 0: vs AI ---
    QWidget *aiPanel = new QWidget(this);
    QFormLayout *aiForm = new QFormLayout(aiPanel);
    m_diffCombo = new QComboBox(aiPanel);
    m_diffCombo->addItem(QStringLiteral("简单"), static_cast<int>(Difficulty::Easy));
    m_diffCombo->addItem(QStringLiteral("普通"), static_cast<int>(Difficulty::Normal));
    m_diffCombo->addItem(QStringLiteral("困难"), static_cast<int>(Difficulty::Hard));
    m_diffCombo->setCurrentIndex(1);
    m_sideCombo = new QComboBox(aiPanel);
    m_sideCombo->addItem(QStringLiteral("红方（先手）"), static_cast<int>(Side::Red));
    m_sideCombo->addItem(QStringLiteral("黑方（后手）"), static_cast<int>(Side::Black));
    aiForm->addRow(QStringLiteral("AI 难度："), m_diffCombo);
    aiForm->addRow(QStringLiteral("执子方："), m_sideCombo);
    m_stack->addWidget(aiPanel);

    // --- Panel 1: host ---
    QWidget *hostPanel = new QWidget(this);
    QFormLayout *hostForm = new QFormLayout(hostPanel);
    m_hostPort = new QSpinBox(hostPanel);
    m_hostPort->setRange(1024, 65535);
    m_hostPort->setValue(9527);
    hostForm->addRow(QStringLiteral("监听端口："), m_hostPort);
    // Show local IPs to help the user share them.
    // NOTE: We must avoid creating QString temporaries that cross the CRT
    // boundary (Qt DLLs use msvcrt, our code uses UCRT). Converting to
    // QByteArray immediately breaks the dependency.
    static char ipBuf[4096];
    memset(ipBuf, 0, sizeof(ipBuf));
    bool foundIp = false;
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        const auto entries = iface.addressEntries();
        for (const auto &entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                !entry.ip().isLoopback()) {
                QByteArray ba = entry.ip().toString().toLatin1();
                if (foundIp) strncat_s(ipBuf, sizeof(ipBuf), ", ", _TRUNCATE);
                strncat_s(ipBuf, sizeof(ipBuf), ba.constData(), _TRUNCATE);
                foundIp = true;
            }
        }
    }
    if (!foundIp) strcpy_s(ipBuf, sizeof(ipBuf), "127.0.0.1");
    hostForm->addRow(QStringLiteral("本机地址："), new QLabel(QString::fromUtf8(ipBuf), hostPanel));
    hostForm->addRow(QString(),
        new QLabel(QStringLiteral("将上面的地址和端口告诉好友即可加入。"), hostPanel));
    m_stack->addWidget(hostPanel);

    // --- Panel 2: join ---
    QWidget *joinPanel = new QWidget(this);
    QFormLayout *joinForm = new QFormLayout(joinPanel);
    m_joinHost = new QLineEdit(QStringLiteral("127.0.0.1"), joinPanel);
    m_joinPort = new QSpinBox(joinPanel);
    m_joinPort->setRange(1024, 65535);
    m_joinPort->setValue(9527);
    joinForm->addRow(QStringLiteral("主机 IP："), m_joinHost);
    joinForm->addRow(QStringLiteral("端口："), m_joinPort);
    m_stack->addWidget(joinPanel);

    // --- Panel 3: hotseat ---
    QWidget *hotPanel = new QWidget(this);
    QVBoxLayout *hotL = new QVBoxLayout(hotPanel);
    hotL->addWidget(new QLabel(
        QStringLiteral("两位玩家轮流在同一台电脑上操作。\n红方先走，棋盘会自动翻转视角。"),
        hotPanel));
    hotL->addStretch();
    m_stack->addWidget(hotPanel);

    // Common: player name + buttons.
    QHBoxLayout *nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(QStringLiteral("你的昵称："), this));
    m_nameEdit = new QLineEdit(QStringLiteral("玩家"), this);
    nameRow->addWidget(m_nameEdit, 1);
    root->addLayout(nameRow);

    QDialogButtonBox *bb = new QDialogButtonBox(this);
    QPushButton *startBtn = bb->addButton(QStringLiteral("开始游戏"), QDialogButtonBox::AcceptRole);
    bb->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);
    startBtn->setDefault(true);
    connect(bb, &QDialogButtonBox::accepted, this, &GameModeDialog::acceptChoice);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);

    onModeChanged(0);
    // Connect signal AFTER construction is fully complete.
    connect(m_list, &QListWidget::currentRowChanged, this, &GameModeDialog::onModeChanged);
}

void GameModeDialog::onModeChanged(int idx)
{
    m_stack->setCurrentIndex(idx);
}

void GameModeDialog::acceptChoice()
{
    m_mode = static_cast<Mode>(m_list->currentRow());
    accept();
}

Difficulty GameModeDialog::aiDifficulty() const
{
    return static_cast<Difficulty>(m_diffCombo->currentData().toInt());
}

Side GameModeDialog::aiHumanSide() const
{
    return static_cast<Side>(m_sideCombo->currentData().toInt());
}

quint16 GameModeDialog::hostPort() const { return static_cast<quint16>(m_hostPort->value()); }
QString GameModeDialog::joinHost() const { return m_joinHost->text().trimmed(); }
quint16 GameModeDialog::joinPort() const { return static_cast<quint16>(m_joinPort->value()); }
QString GameModeDialog::playerName() const { return m_nameEdit->text().trimmed(); }

} // namespace chess
