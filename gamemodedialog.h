#ifndef GAMEMODEDIALOG_H
#define GAMEMODEDIALOG_H

#include <QDialog>
#include "ai.h"

class QComboBox;
class QSpinBox;
class QLineEdit;
class QStackedWidget;
class QListWidget;
class QLabel;

namespace chess {

// Top-level "what do you want to do?" dialog. The user first picks a mode
// (single-player vs AI, host a network game, join a network game, two-player
// hotseat), and the relevant sub-panel collects the rest of the options.
class GameModeDialog : public QDialog
{
    Q_OBJECT
public:
    enum Mode { VsAI = 0, HostNet = 1, JoinNet = 2, Hotseat = 3, Cancelled = -1 };

    explicit GameModeDialog(QWidget *parent = nullptr);

    Mode selectedMode() const { return m_mode; }

    // Options read after the dialog is accepted.
    Difficulty aiDifficulty() const;
    Side aiHumanSide() const;     // which color the human plays vs AI
    quint16 hostPort() const;
    QString joinHost() const;
    quint16 joinPort() const;
    QString playerName() const;

private slots:
    void onModeChanged(int idx);
    void acceptChoice();

private:
    Mode m_mode = Cancelled;
    QListWidget *m_list = nullptr;
    QStackedWidget *m_stack = nullptr;

    // vs AI panel
    QComboBox *m_diffCombo = nullptr;
    QComboBox *m_sideCombo = nullptr;

    // host panel
    QSpinBox *m_hostPort = nullptr;

    // join panel
    QLineEdit *m_joinHost = nullptr;
    QSpinBox *m_joinPort = nullptr;

    QLineEdit *m_nameEdit = nullptr;
};

} // namespace chess
#endif // GAMEMODEDIALOG_H
