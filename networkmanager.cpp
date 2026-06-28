#include "networkmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QHostAddress>
#include <QNetworkInterface>

namespace chess {

NetworkManager::NetworkManager(QObject *parent) : QObject(parent) {}

NetworkManager::~NetworkManager() { disconnect(); }

bool NetworkManager::host(quint16 port)
{
    disconnect();
    m_role = Role::Host;
    if (!m_server) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
    }
    if (!m_server->listen(QHostAddress::Any, port)) {
        m_role = Role::None;
        emit errorOccurred(tr("无法监听端口 %1：%2")
                               .arg(port)
                               .arg(m_server->errorString()));
        return false;
    }
    return true;
}

bool NetworkManager::join(const QString &host, quint16 port)
{
    disconnect();
    m_role = Role::Client;
    m_sock = new QTcpSocket(this);
    connect(m_sock, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_sock, &QTcpSocket::disconnected, this, &NetworkManager::onSocketDisconnected);
    connect(m_sock, &QAbstractSocket::errorOccurred, this, &NetworkManager::onSocketError);
    m_sock->connectToHost(host, port);
    return true;
}

void NetworkManager::disconnect()
{
    if (m_sock) {
        m_sock->abort();
        delete m_sock;
        m_sock = nullptr;
    }
    if (m_server) {
        m_server->close();
        // m_server is parented to this; we keep it around for reuse.
    }
    m_rx.clear();
    m_role = Role::None;
}

bool NetworkManager::isConnected() const
{
    return m_sock && m_sock->state() == QAbstractSocket::ConnectedState;
}

void NetworkManager::sendJson(const QJsonObject &obj)
{
    if (!isConnected()) return;
    QJsonDocument doc(obj);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');
    m_sock->write(data);
    m_sock->flush();
}

void NetworkManager::sendMove(int fr, int fc, int tr, int tc)
{
    sendJson(QJsonObject{
        {"t", "move"},
        {"fr", fr}, {"fc", fc}, {"tr", tr}, {"tc", tc}
    });
}

void NetworkManager::sendChat(const QString &text)
{
    sendJson(QJsonObject{ {"t", "chat"}, {"text", text} });
}

void NetworkManager::sendResign()      { sendJson(QJsonObject{ {"t", "resign"} }); }
void NetworkManager::sendRematch()     { sendJson(QJsonObject{ {"t", "rematch"} }); }
void NetworkManager::sendHello(const QString &name)
{
    sendJson(QJsonObject{ {"t", "hello"}, {"name", name} });
}

void NetworkManager::onNewConnection()
{
    // Accept only the first peer; ignore the rest.
    while (m_server->hasPendingConnections()) {
        QTcpSocket *next = m_server->nextPendingConnection();
        if (!m_sock) {
            m_sock = next;
            m_sock->setParent(this);
            connect(m_sock, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
            connect(m_sock, &QTcpSocket::disconnected, this, &NetworkManager::onSocketDisconnected);
            connect(m_sock, &QAbstractSocket::errorOccurred, this, &NetworkManager::onSocketError);
            emit connected();
        } else {
            next->disconnectFromHost();
            next->deleteLater();
        }
    }
}

void NetworkManager::onReadyRead()
{
    if (!m_sock) return;
    m_rx.append(m_sock->readAll());
    int idx;
    while ((idx = m_rx.indexOf('\n')) >= 0) {
        QByteArray line = m_rx.left(idx);
        m_rx.remove(0, idx + 1);
        processLine(line);
    }
}

void NetworkManager::processLine(const QByteArray &line)
{
    if (line.trimmed().isEmpty()) return;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError) return;
    QJsonObject o = doc.object();
    QString t = o.value("t").toString();
    if (t == "move") {
        int fr = o.value("fr").toInt(-1);
        int fc = o.value("fc").toInt(-1);
        int tr = o.value("tr").toInt(-1);
        int tc = o.value("tc").toInt(-1);
        if (fr >= 0 && fc >= 0 && tr >= 0 && tc >= 0)
            emit moveReceived(fr, fc, tr, tc);
    } else if (t == "chat") {
        emit chatReceived(o.value("text").toString());
    } else if (t == "resign") {
        emit resignReceived();
    } else if (t == "rematch") {
        emit rematchReceived();
    } else if (t == "hello") {
        emit helloReceived(o.value("name").toString());
    }
}

void NetworkManager::onSocketDisconnected()
{
    emit disconnected();
    if (m_sock) {
        m_sock->deleteLater();
        m_sock = nullptr;
    }
}

void NetworkManager::onSocketError(QAbstractSocket::SocketError)
{
    if (m_sock)
        emit errorOccurred(m_sock->errorString());
    else
        emit errorOccurred(tr("网络错误"));
}

} // namespace chess
