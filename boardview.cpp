#include "boardview.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QRadialGradient>
#include <cmath>

namespace chess {

// The internal board uses r=0 for Black's back rank and r=9 for Red's.
// When the local player is Red we want Red at the bottom, so we flip rows.
static inline int viewRow(int r, Side local)
{
    return (local == Side::Red) ? (9 - r) : r;
}

BoardView::BoardView(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(420, 480);
}

void BoardView::computeLayout()
{
    int w = width();
    int h = height();
    // We have 9 files (8 gaps) and 10 ranks (9 gaps).
    int availW = w - 2 * m_margin;
    int availH = h - 2 * m_margin;
    m_cellW = availW / 8;
    m_cellH = availH / 9;
    // Keep squares roughly square but allow some stretch.
    if (m_cellW < 30) m_cellW = 30;
    if (m_cellH < 30) m_cellH = 30;
    int cell = std::min(m_cellW, m_cellH);
    m_cellW = cell;
    m_cellH = cell;
    m_pieceR = cell / 2 - 3;
    if (m_pieceR < 12) m_pieceR = 12;
}

QPoint BoardView::cellCenter(int r, int c) const
{
    int vr = viewRow(r, m_localSide);
    int x = m_margin + c * m_cellW;
    int y = m_margin + vr * m_cellH;
    return QPoint(x, y);
}

bool BoardView::pointToCell(const QPoint &pt, int &r, int &c) const
{
    // Find the closest intersection within half a cell.
    int bestR = -1, bestC = -1;
    int bestDist = m_cellW; // threshold
    for (int rr = 0; rr < 10; ++rr) {
        for (int cc = 0; cc < 9; ++cc) {
            QPoint ctr = cellCenter(rr, cc);
            int dx = pt.x() - ctr.x();
            int dy = pt.y() - ctr.y();
            int d = std::max(std::abs(dx), std::abs(dy));
            if (d < bestDist) {
                bestDist = d;
                bestR = rr; bestC = cc;
            }
        }
    }
    if (bestR < 0) return false;
    r = bestR; c = bestC;
    return true;
}

QSize BoardView::sizeHint() const
{
    return QSize(560, 640);
}

void BoardView::resizeEvent(QResizeEvent *) { computeLayout(); }

void BoardView::setSelected(int r, int c) { m_selR = r; m_selC = c; update(); }
void BoardView::clearSelection() { m_selR = -1; m_selC = -1; update(); }
void BoardView::setHints(const std::vector<Move> &moves) { m_hints = moves; update(); }
void BoardView::clearHints() { m_hints.clear(); update(); }
void BoardView::setLastMove(int fr, int fc, int tr, int tc)
{ m_hasLast = true; m_lastFr = fr; m_lastFc = fc; m_lastTr = tr; m_lastTc = tc; update(); }
void BoardView::clearLastMove() { m_hasLast = false; update(); }

// Draw the wooden board background and grid lines, palace, river, labels.
static void drawBoard(QPainter &p, const QRect &r, int margin, int cellW, int cellH)
{
    // Background.
    QLinearGradient bg(r.topLeft(), r.bottomRight());
    bg.setColorAt(0.0, QColor(244, 214, 154));
    bg.setColorAt(1.0, QColor(224, 188, 124));
    p.fillRect(r, bg);

    QPen linePen(QColor(60, 40, 20), 2);
    p.setPen(linePen);

    int leftX = margin;
    int rightX = margin + 8 * cellW;
    int topY = margin;
    int botY = margin + 9 * cellH;

    // Horizontal lines (10 of them).
    for (int i = 0; i < 10; ++i) {
        int y = margin + i * cellH;
        p.drawLine(leftX, y, rightX, y);
    }
    // Vertical lines: full at the two edges, broken across the river in between.
    p.drawLine(leftX, topY, leftX, botY);
    p.drawLine(rightX, topY, rightX, botY);
    int riverTop = margin + 4 * cellH;
    int riverBot = margin + 5 * cellH;
    for (int i = 1; i < 8; ++i) {
        int x = margin + i * cellW;
        p.drawLine(x, topY, x, riverTop);
        p.drawLine(x, riverBot, x, botY);
    }

    // Palaces (diagonals) for both sides.
    auto drawPalace = [&](int r0, int c0, int r1, int c1) {
        p.drawLine(margin + c0 * cellW, margin + r0 * cellH,
                   margin + c1 * cellW, margin + r1 * cellH);
    };
    // Top palace (rows 0..2, cols 3..5).
    drawPalace(0, 3, 2, 5);
    drawPalace(0, 5, 2, 3);
    // Bottom palace (rows 7..9).
    drawPalace(7, 3, 9, 5);
    drawPalace(7, 5, 9, 3);

    // River text.
    QFont riverFont(QStringLiteral("Microsoft YaHei"), std::max(10, cellW / 4), QFont::Bold);
    p.setFont(riverFont);
    p.setPen(QColor(90, 60, 30));
    int midY = (riverTop + riverBot) / 2;
    QFontMetrics fm(riverFont);
    QString leftText = QStringLiteral("楚 河");
    QString rightText = QStringLiteral("漢 界");
    p.drawText(QRect(leftX + cellW / 2, midY - fm.height() / 2, cellW * 2, fm.height()),
               Qt::AlignCenter, leftText);
    p.drawText(QRect(rightX - cellW * 5 / 2, midY - fm.height() / 2, cellW * 2, fm.height()),
               Qt::AlignCenter, rightText);
}

// Small "L" tick marks drawn at cannon and pawn starting squares.
static void drawTicks(QPainter &p, int margin, int cellW, int cellH)
{
    auto tick = [&](int col, int row, int dx, int dy, int len) {
        int cx = margin + col * cellW;
        int cy = margin + row * cellH;
        // outer corner offset so ticks sit just outside the intersection.
        int ox = cx + dx * 6;
        int oy = cy + dy * 6;
        p.drawLine(ox, oy, ox + dx * len, oy);
        p.drawLine(ox, oy, ox, oy + dy * len);
    };
    QPen pen(QColor(60, 40, 20), 1.5);
    p.setPen(pen);
    struct Pos { int c, r; };
    Pos marks[] = {
        {1,2},{7,2},   // black cannons
        {1,7},{7,7},   // red cannons
        {0,3},{2,3},{4,3},{6,3},{8,3}, // black pawns
        {0,6},{2,6},{4,6},{6,6},{8,6}  // red pawns
    };
    for (auto &m : marks) {
        bool leftEdge = (m.c == 0);
        bool rightEdge = (m.c == 8);
        if (!leftEdge)  { tick(m.c, m.r, -1, -1, 5); tick(m.c, m.r, -1, +1, 5); }
        if (!rightEdge) { tick(m.c, m.r, +1, -1, 5); tick(m.c, m.r, +1, +1, 5); }
    }
}

// The Chinese character shown for each piece.
static QString pieceChar(Piece p)
{
    switch (p) {
    case Piece::RK: return QStringLiteral("帥");
    case Piece::RA: return QStringLiteral("仕");
    case Piece::RB: return QStringLiteral("相");
    case Piece::RN: return QStringLiteral("馬");
    case Piece::RR: return QStringLiteral("車");
    case Piece::RC: return QStringLiteral("炮");
    case Piece::RP: return QStringLiteral("兵");
    case Piece::BK: return QStringLiteral("將");
    case Piece::BA: return QStringLiteral("士");
    case Piece::BB: return QStringLiteral("象");
    case Piece::BN: return QStringLiteral("馬");
    case Piece::BR: return QStringLiteral("車");
    case Piece::BC: return QStringLiteral("砲");
    case Piece::BP: return QStringLiteral("卒");
    default: return QString();
    }
}

static void drawPiece(QPainter &p, Piece piece, const QPoint &center, int radius)
{
    if (piece == Piece::None) return;
    bool red = isRed(piece);

    QRectF outer(center.x() - radius, center.y() - radius,
                 radius * 2, radius * 2);

    // Soft drop shadow.
    QRadialGradient shadow(QPointF(center.x(), center.y() + radius * 0.35), radius * 1.2);
    shadow.setColorAt(0.0, QColor(0, 0, 0, 90));
    shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setBrush(QBrush(shadow));
    p.setPen(Qt::NoPen);
    p.drawEllipse(outer.translated(0, radius * 0.18));

    // Disc body.
    QRadialGradient body(center, radius);
    body.setColorAt(0.0, QColor(253, 240, 210));
    body.setColorAt(0.7, QColor(238, 216, 168));
    body.setColorAt(1.0, QColor(210, 178, 120));
    p.setBrush(QBrush(body));
    p.setPen(QPen(QColor(120, 90, 50), 1.5));
    p.drawEllipse(outer);

    // Inner ring.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(red ? QColor(170, 30, 30) : QColor(20, 20, 20), 1.5));
    p.drawEllipse(outer.adjusted(radius * 0.18, radius * 0.18,
                                 -radius * 0.18, -radius * 0.18));

    // Character.
    QFont f(QStringLiteral("Microsoft YaHei"), std::max(11, radius), QFont::Black);
    p.setFont(f);
    p.setPen(red ? QColor(170, 20, 20) : QColor(15, 15, 15));
    p.drawText(outer, Qt::AlignCenter, pieceChar(piece));
}

void BoardView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    computeLayout();

    drawBoard(p, rect(), m_margin, m_cellW, m_cellH);
    drawTicks(p, m_margin, m_cellW, m_cellH);

    // Last-move highlight.
    if (m_hasLast) {
        QColor hl(255, 215, 0, 90);
        for (auto cell : { QPoint(m_lastFr, m_lastFc), QPoint(m_lastTr, m_lastTc) }) {
            QPoint c = cellCenter(cell.x(), cell.y());
            QRect r(c.x() - m_pieceR - 2, c.y() - m_pieceR - 2,
                    (m_pieceR + 2) * 2, (m_pieceR + 2) * 2);
            p.setBrush(hl);
            p.setPen(Qt::NoPen);
            p.drawEllipse(r);
        }
    }

    // Selection highlight.
    if (m_selR >= 0) {
        QPoint c = cellCenter(m_selR, m_selC);
        p.setBrush(QColor(80, 160, 255, 90));
        p.setPen(QPen(QColor(40, 110, 220), 2));
        QRect r(c.x() - m_pieceR - 3, c.y() - m_pieceR - 3,
                (m_pieceR + 3) * 2, (m_pieceR + 3) * 2);
        p.drawEllipse(r);
    }

    // Pieces.
    if (m_board) {
        for (int r = 0; r < 10; ++r) {
            for (int c = 0; c < 9; ++c) {
                Piece pc = m_board->at(r, c);
                if (pc == Piece::None) continue;
                drawPiece(p, pc, cellCenter(r, c), m_pieceR);
            }
        }
    }

    // Hint dots for legal destinations.
    for (const auto &m : m_hints) {
        QPoint c = cellCenter(m.tr, m.tc);
        bool capture = (m.captured != Piece::None) || (m_board && m_board->at(m.tr, m.tc) != Piece::None);
        if (capture) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(220, 60, 60), 3));
            QRect r(c.x() - m_pieceR - 4, c.y() - m_pieceR - 4,
                    (m_pieceR + 4) * 2, (m_pieceR + 4) * 2);
            p.drawEllipse(r);
        } else {
            p.setBrush(QColor(60, 170, 80, 170));
            p.setPen(Qt::NoPen);
            p.drawEllipse(c, m_pieceR / 3, m_pieceR / 3);
        }
    }
}

void BoardView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    int r, c;
    if (!pointToCell(e->pos(), r, c)) { QWidget::mousePressEvent(e); return; }

    if (m_selR < 0) {
        // First click: select our own piece (if any).
        if (m_board) {
            Piece pc = m_board->at(r, c);
            if (pc != Piece::None && sideOf(pc) == m_board->sideToMove()) {
                setSelected(r, c);
                // Provide legal-move hints for nicer UX.
                std::vector<Move> all = m_board->generateMoves();
                std::vector<Move> mine;
                for (auto &m : all)
                    if (m.fr == r && m.fc == c) mine.push_back(m);
                setHints(mine);
            }
        }
    } else {
        // Second click: attempt to move. If clicking the same square, deselect.
        if (r == m_selR && c == m_selC) {
            clearSelection();
            clearHints();
        } else {
            emit moveRequested(m_selR, m_selC, r, c);
        }
    }
}

} // namespace chess
