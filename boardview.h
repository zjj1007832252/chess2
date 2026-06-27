#ifndef BOARDVIEW_H
#define BOARDVIEW_H

#include <QWidget>
#include "board.h"

namespace chess {

// Widget that paints the board and pieces, and converts mouse clicks into
// square coordinates. Emits a moveRequested signal when the user completes a
// from->to selection (the MainWindow decides whether to apply it).
class BoardView : public QWidget
{
    Q_OBJECT
public:
    explicit BoardView(QWidget *parent = nullptr);

    void setBoard(const Board *b) { m_board = b; update(); }
    void setLocalSide(Side s) { m_localSide = s; update(); }
    Side localSide() const { return m_localSide; }

    // Visual hints driven by the controller.
    void setSelected(int r, int c);                 // highlight a selected piece
    void clearSelection();
    void setHints(const std::vector<Move> &moves);  // legal targets to highlight
    void clearHints();
    void setLastMove(int fr, int fc, int tr, int tc);
    void clearLastMove();

    // Coordinate conversion (used by MainWindow for click handling).
    QPoint cellCenter(int r, int c) const;
    bool pointToCell(const QPoint &pt, int &r, int &c) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

signals:
    // The user clicked `from`, then `to`.
    void moveRequested(int fr, int fc, int tr, int tc);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void computeLayout();

    const Board *m_board = nullptr;
    Side m_localSide = Side::Red;   // which side is drawn at the bottom

    int m_margin = 30;              // pixels of padding around the grid
    int m_cellW = 60;               // horizontal spacing between files
    int m_cellH = 60;               // vertical spacing between ranks
    int m_pieceR = 26;              // piece radius

    int m_selR = -1, m_selC = -1;
    std::vector<Move> m_hints;      // legal destination squares for the selection
    bool m_hasLast = false;
    int m_lastFr = 0, m_lastFc = 0, m_lastTr = 0, m_lastTc = 0;
};

} // namespace chess
#endif // BOARDVIEW_H
