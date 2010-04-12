#include "ChatWindow.h"
#include "QXmppClient.h"
#include "QXmppMessage.h"
#include "QXmppUtils.h"
#include <QTime>
#include <QDomDocument>
#include "XmppMessage.h"
#include <QXmppRoster.h>
#include <QCloseEvent>
#include <QTimer>

ChatWindow::ChatWindow(QWidget *parent)
    : QMainWindow(parent), m_selfState(QXmppMessage::Active), m_pausedTimer(new QTimer),
    m_inactiveTimer(new QTimer), m_goneTimer(new QTimer)
{
    ui.setupUi(this);
    ui.messageEdit->setFocus();

    connect(ui.sendButton, SIGNAL(clicked()),
            this, SLOT(sendMessage()));
    connect(ui.messageEdit, SIGNAL(textChanged()),
            this, SLOT(sendComposing()));
    connect(m_pausedTimer, SIGNAL(timeout()),
            this, SLOT(pausedTimeout()));
    connect(m_inactiveTimer, SIGNAL(timeout()),
            this, SLOT(inactiveTimeout()));
    connect(m_goneTimer, SIGNAL(timeout()),
            this, SLOT(goneTimeout()));

    setAttribute(Qt::WA_QuitOnClose, false);
}

void ChatWindow::setClient(QXmppClient *client)
{
    m_client = client;
}

void ChatWindow::setJid(QString jid)
{
    m_jid = jid;
}

void ChatWindow::appendMessage(const QXmppMessage &o_message)
{
    XmppMessage message(o_message);
    changeState(message.state());
    if (!message.body().isEmpty()){
        //QString bareJid = jidToBareJid(message.from()); 
        ui.messageBrowser->append(QString("%1 %2").arg(message.from()).arg(QTime::currentTime().toString()));
        if (message.html().isEmpty()) {
            ui.messageBrowser->append(message.body());
        } else {
            ui.messageBrowser->append(message.html());
        }
    }
}

void ChatWindow::sendMessage()
{
    XmppMessage message(m_client->getConfiguration().jid(),
                        m_jid,
                        ui.messageEdit->toPlainText());
    message.setHtml(ui.messageEdit->toHtml());
    m_client->sendPacket(message);

    QString c_bareJid = m_client->getConfiguration().jidBare();
    ui.messageBrowser->append(QString("%1 %2").arg(c_bareJid).arg(QTime::currentTime().toString()));
    ui.messageBrowser->append(ui.messageEdit->toHtml());
    ui.messageEdit->clear();
    m_selfState = QXmppMessage::Active;
}

void ChatWindow::changeState(QXmppMessage::State state)
{
    QString stateStr;
    switch (state) {
        case QXmppMessage::None :
            stateStr = "None";
            break;
        case QXmppMessage::Active :
            stateStr = "Active";
            break;
        case QXmppMessage::Inactive :
            stateStr = "Inactive";
            break;
        case QXmppMessage::Gone :
            stateStr = "Gone";
            break;
        case QXmppMessage::Composing :
            stateStr = "Composing";
            break;
        case QXmppMessage::Paused :
            stateStr = "Paused";
            break;
        default:
            break;
    }
    ui.stateLabel->setText(QString("State: %1").arg(stateStr));
}

void ChatWindow::sendComposing()
{
    // reset timer
    m_pausedTimer->start(30000);
    m_inactiveTimer->start(120000);
    m_goneTimer->start(600000);

    changeSelfState(QXmppMessage::Composing);
}

void ChatWindow::changeSelfState(QXmppMessage::State state)
{
    if (m_selfState != state) {
        m_selfState = state;

        QString resource = jidToResource(m_jid);
        QString bareJid = jidToBareJid(m_jid);
        XmppMessage message(m_client->getConfiguration().jid(),
                            m_jid);
        message.setState(state);
        if (resource.isEmpty()) {
            // if breaJid at less have one resource
            if (!m_client->getRoster().getAllPresencesForBareJid(bareJid).isEmpty()) {
                m_client->sendPacket(message);
            }
        } else {
            // if resource no unavable
            if (!m_client->getRoster().getPresence(bareJid, resource).from().isEmpty()) {
                m_client->sendPacket(message);
            }
        }
    }
}

void ChatWindow::closeEvent(QCloseEvent *event)
{
    changeSelfState(QXmppMessage::Gone);
    event->accept();
}

void ChatWindow::pausedTimeout()
{
    changeSelfState(QXmppMessage::Paused);
    m_pausedTimer->stop();
}
void ChatWindow::inactiveTimeout()
{
    changeSelfState(QXmppMessage::Inactive);
    m_inactiveTimer->stop();
}
void ChatWindow::goneTimeout()
{
    changeSelfState(QXmppMessage::Gone);
    m_goneTimer->stop();
}
