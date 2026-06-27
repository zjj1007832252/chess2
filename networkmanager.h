#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>

namespace chess {

// A small, line-based protocol over TCP for two-player games.
//
// Messages are UTF-8 JSON objects terminated by '\n', e.g.
//   {"t":"move","fr":7,"fc":4,"tr":7,"tc":4}
//   {"t":"hello","name":"Alice"}
//   {"t":"chat","text":"hi"}
//   {"t":"resign"}
//   {"t":"rematch"}
//
// The host (server) and the client (joiner) are symmetric peers once connected:
// both can send and receive moves. The host always plays Red and moves first,
// the joining client plays Black.
class NetworkManager : public QObject
{
    Q_OBJECT
public:
    enum class Role { None, Host, Client };
    Q_ENUM(Role)

    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    // Start hosting on the given port. Returns true if listening succeeded.
    bool host(quint16 port);
    // Connect to host:port. Returns true if a connection attempt was started.
    bool join(const QString &host, quint16 port);

    // Send the local player's move to the peer.
    void sendMove(int fr, int fc, int tr, int tc);
    void sendChat(const QString &text);
    void sendResign();
    void sendRematch();
    void sendHello(const QString &name);

    bool isConnected() const;
    Role role() const { return m_role; }

public slots:
    void disconnect();

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &msg);
    void moveReceived(int fr, int fc, int tr, int tc);
    void chatReceived(const QString &text);
    void resignReceived();
    void rematchReceived();
    void helloReceived(const QString &name);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError e);

private:
    void sendJson(const class QJsonObject &obj);
    void processLine(const QByteArray &line);

    QTcpServer *m_server = nullptr;
    QTcpSocket *m_sock = nullptr;     // active peer socket (accepted or connected)
    QByteArray m_rx;                  // receive buffer
    Role m_role = Role::None;
};

} // namespace chess
#endif // NETWORKMANAGER_H
