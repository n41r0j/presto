#include <QDir>
#include "LightningModel.h"
#include "./3rdparty/qjsonrpc/src/qjsonrpcservicereply.h"

PeersModel *LightningModel::peersModel() const
{
    return m_peersModel;
}

PaymentsModel *LightningModel::paymentsModel() const
{
    return m_paymentsModel;
}

WalletModel *LightningModel::walletModel() const
{
    return m_walletModel;
}

InvoicesModel *LightningModel::invoicesModel() const
{
    return m_invoicesModel;
}

QString LightningModel::id() const
{
    return m_id;
}

int LightningModel::port() const
{
    return m_port;
}

QString LightningModel::address() const
{
    return m_address;
}

QString LightningModel::version() const
{
    return m_version;
}

int LightningModel::blockheight() const
{
    return m_blockheight;
}

QString LightningModel::network() const
{
    return m_network;
}

void LightningModel::updateInfo()
{
    QJsonRpcMessage message = QJsonRpcMessage::createRequest("getinfo", QJsonValue());
    QJsonRpcServiceReply* reply = m_rpcSocket->sendMessage(message);
    QObject::connect(reply, &QJsonRpcServiceReply::finished, this, &LightningModel::updateInfoRequestFinished);
}

void LightningModel::launchDaemon()
{
    QString program = "/home/igor/Code/presto/3rdparty/lightning/lightningd/lightningd";
    QStringList arguments;
    arguments << "--network=testnet";

    m_lightningDaemonProcess = new QProcess();
    m_lightningDaemonProcess->start(program, arguments);
    m_lightningDaemonProcess->waitForStarted();
}

QString LightningModel::serverName() const
{
    return m_serverName;
}

void LightningModel::setServerName(const QString &serverName)
{
    m_serverName = serverName;
}

void LightningModel::updateInfoRequestFinished()
{
    QJsonRpcServiceReply *reply = static_cast<QJsonRpcServiceReply *>(sender());
    QJsonRpcMessage message = reply->response();

    if (message.type() == QJsonRpcMessage::Error)
    {
        //emit errorString(message.toObject().value("error").toObject().value("message").toString());
    }

    if (message.type() == QJsonRpcMessage::Response)
    {
        QJsonObject resultsObject = message.toObject().value("result").toObject();

        m_address = resultsObject.value("address").toString();
        m_blockheight = resultsObject.value("blockheight").toInt();
        m_id = resultsObject.value("id").toString();
        m_network = resultsObject.value("network").toString();
        m_port = resultsObject.value("port").toInt();
        m_version = resultsObject.value("version").toString();

        emit infoChanged();
    }
}

LightningModel::LightningModel(QString serverName, QObject *parent) {
    {
        Q_UNUSED(parent);

        if (serverName.isEmpty()) {
            m_serverName = QDir::homePath() + "/.lightning/lightning-rpc";
        }
        else {
            m_serverName = serverName;
        }

        m_address = QString();
        m_blockheight = 0;
        m_id = QString();
        m_network = QString();
        m_port = 0;
        m_version = QString();

        m_unixSocket = new QLocalSocket();
        m_rpcSocket = new QJsonRpcSocket(m_unixSocket);

        //        QObject::connect(m_unixSocket, &QLocalSocket::connected,
        //                         this, &LightningModel::rpcConnected);

        QObject::connect(m_unixSocket, SIGNAL(error(QLocalSocket::LocalSocketError)),
                         this, SLOT(unixSocketError(QLocalSocket::LocalSocketError)));

        QObject::connect(m_rpcSocket, &QJsonRpcAbstractSocket::messageReceived, this, &LightningModel::rpcMessageReceived);

        m_unixSocket->connectToServer(m_serverName);

        if (m_unixSocket->waitForConnected())
        {
            rpcConnected();
        }
        else
        {
            rpcNotConnected();
        }

    }
}

void LightningModel::rpcConnected()
{
    m_peersModel = new PeersModel(m_rpcSocket);
    m_paymentsModel = new PaymentsModel(m_rpcSocket);
    m_walletModel = new WalletModel(m_rpcSocket);
    m_invoicesModel = new InvoicesModel(m_rpcSocket);

    updateInfo();

    m_updatesTimer = new QTimer();

    // Update the models every 15 secs
    m_updatesTimer->setInterval(15000);
    m_updatesTimer->setSingleShot(false);
    QObject::connect(m_updatesTimer, &QTimer::timeout, this, &LightningModel::updateModels);
    m_updatesTimer->start();
}

void LightningModel::rpcNotConnected()
{
    // No daemon so lets launch our own
    launchDaemon();

    // wait
    int ms = 5000;
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
    nanosleep(&ts, NULL);


    // Try to connect
    m_unixSocket->connectToServer(m_serverName);
    if (m_unixSocket->waitForConnected())
    {
        rpcConnected();
    }
    else
    {

    }
}

void LightningModel::rpcMessageReceived(QJsonRpcMessage message)
{
    qDebug() << message.toJson();
}

void LightningModel::unixSocketError(QLocalSocket::LocalSocketError socketError)
{
    qDebug() << socketError;
}

void LightningModel::updateModels()
{
    m_paymentsModel->updatePayments();
    m_walletModel->updateFunds();
    m_invoicesModel->updateInvoices();
    updateInfo();
    // This calls needs to go last cause it hates concurrency
    m_peersModel->updatePeers();
}