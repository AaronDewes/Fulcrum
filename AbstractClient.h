#ifndef ABSTRACTCLIENT_H
#define ABSTRACTCLIENT_H

#include <QObject>
#include <atomic>
#include "Common.h"
#include <QTcpSocket>

class QTimer;

class AbstractClient : public QObject
{
    Q_OBJECT
public:
    explicit AbstractClient(qint64 id, QObject *parent = nullptr);

    const qint64 MAX_BUFFER = 20000000; // 20MB, may override this in derived classes

    const qint64 id;

    /// true if we are connected
    virtual bool isGood() const;
    /// true if we are connected but haven't received any response in some time
    virtual bool isStale() const;
    /// true if we got a malformed reply from the server
    virtual bool isBad() const { return status == Bad; }

signals:
    void lostConnection(AbstractClient *);
    /// call (emit) this to send data to the other end. connected to do_write(). This is a low-level function
    /// subclasses should create their own high-level protocol-level signals.
    void send(QByteArray);

protected slots:
    virtual void on_readyRead() = 0; /**< Implement in subclasses -- required to read data */

protected:

    enum Status {
        NotConnected = 0,
        Connecting,
        Connected,
        Bad
    };

    void socketConnectSignals(); ///< call this from derived classes to connect socket error and stateChanged to this

    std::atomic<Status> status = NotConnected;
    /// timestamp in ms from Util::getTime() when the server was last good
    /// (last communicated a sensible message, pinged, etc)
    std::atomic<qint64> lastGood = 0LL; ///< update this in derived classes.

    std::atomic<qint64> nSent = 0ULL, ///< this get updated in this class in do_write()
                        nReceived = 0ULL;  ///< update this in derived classes in your on_readyRead()

    static constexpr qint64 reconnectTime = 2*60*1000; /// retry every 2 mins

    static constexpr int pingtime_ms = 60*1000;  /// send server.ping if idle for >1 min
    static constexpr qint64 stale_threshold = reconnectTime;
    QTcpSocket *socket = nullptr; ///< this should only ever be touched in our thread
    QByteArray writeBackLog = ""; ///< if this grows beyond a certain size, we should kill the connection
    QTimer *pingTimer = nullptr;

    virtual QString prettyName(bool dontTouchSocket=false) const; ///< called only from our thread otherwise it may crash because it touches 'socket'

    virtual void do_ping(); /**< Reimplement in subclasses to send a ping. Default impl. does nothing. */

    virtual void on_connected(); ///< overrides should call this base implementation and chain to it

    bool do_write(const QByteArray & = "");
    void boilerplate_disconnect(); /// does a socket->abort, sets status

private slots:
    void on_pingTimer();
    void on_bytesWritten();
    void on_error(QAbstractSocket::SocketError);
    void on_socketState(QAbstractSocket::SocketState);
private:
    void start_pingTimer();
    void kill_pingTimer();
};

#endif // ABSTRACTCLIENT_H