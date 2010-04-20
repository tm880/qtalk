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
#include <MessageEdit.h>
#include <QStatusBar>
#include <QPushButton>
#include "ContactInfoDialog.h"
#include <QFileDialog>
#include <QDesktopServices>

ChatWindow::ChatWindow(QWidget *parent) :
    QMainWindow(parent),
    m_selfState(QXmppMessage::Active),
    m_pausedTimer(new QTimer),
    m_inactiveTimer(new QTimer),
    m_goneTimer(new QTimer),
    m_statusBar(new QStatusBar),
    m_sendButton(new QPushButton),
    m_sendTip(new QLabel)
{
    ui.setupUi(this);

    m_editor = new MessageEdit();
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(m_editor);
    layout->setMargin(0);
    ui.editorWarpwidget->setLayout(layout);
    m_editor->setFocus();

    m_sendButton->setText("Send");
    m_sendButton->setFixedHeight(m_statusBar->sizeHint().height());
    m_statusBar->addPermanentWidget(m_sendTip);
    m_statusBar->addPermanentWidget(m_sendButton);
    m_statusBar->setSizeGripEnabled(false);
    setStatusBar(m_statusBar);

    connect(m_sendButton, SIGNAL(clicked()),
            this, SLOT(sendMessage()));
    connect(m_editor, SIGNAL(textChanged()),
            this, SLOT(sendComposing()));
    connect(m_pausedTimer, SIGNAL(timeout()),
            this, SLOT(pausedTimeout()));
    connect(m_inactiveTimer, SIGNAL(timeout()),
            this, SLOT(inactiveTimeout()));
    connect(m_goneTimer, SIGNAL(timeout()),
            this, SLOT(goneTimeout()));
    connect(ui.detailButton, SIGNAL(clicked()),
            this, SLOT(openContactInfoDialog()) );

    // action
    connect(ui.actionSendFile, SIGNAL(triggered()),
            this, SLOT(sendFileSlot()) );

    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose, true);
}

void ChatWindow::setClient(QXmppClient *client)
{
    m_client = client;
}

void ChatWindow::setJid(QString jid)
{
    m_jid = jid;
    QXmppRoster::QXmppRosterEntry entry = m_client->getRoster().getRosterEntry(jidToBareJid(jid));
    ui.name->setText(entry.name());
    ui.jid->setText(jid);
    if (m_client->getRoster().getResources(jidToBareJid(jid)).isEmpty())
        ui.photo->setPixmap(QPixmap(":/images/user-identity-grey-100.png"));
    else
        ui.photo->setPixmap(QPixmap(":/images/user-identity-100.png"));
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

void ChatWindow::readPref(Preferences *pref)
{
    if (pref->enterToSendMessage) {
        m_sendButton->setShortcut(QKeySequence("Return"));
        m_sendTip->setText("Enter");
    } else {
        m_sendButton->setShortcut(QKeySequence("Ctrl+Return"));
        m_sendTip->setText("Ctrl+Enter");
    }
    m_editor->setIgnoreEnter(pref->enterToSendMessage);
}

void ChatWindow::setVCard(QXmppVCard vCard)
{
    m_vCard = vCard;
    if (!vCard.photo().isEmpty())
        ui.photo->setPixmap(QPixmap::fromImage(vCard.photoAsImage()));
}

void ChatWindow::sendMessage()
{
    if (m_editor->toPlainText().isEmpty())
        return;
    XmppMessage message(m_client->getConfiguration().jid(),
                        m_jid,
                        m_editor->toPlainText());
    message.setHtml(m_editor->toHtml());
    m_client->sendPacket(message);

    QString c_bareJid = m_client->getConfiguration().jidBare();
    ui.messageBrowser->append(QString("%1 %2").arg(c_bareJid).arg(QTime::currentTime().toString()));
    ui.messageBrowser->append(m_editor->toHtml());
    m_editor->clear();
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
    m_statusBar->showMessage(QString("State: %1").arg(stateStr));
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

void ChatWindow::openContactInfoDialog()
{
    if (m_contactInfoDialog == NULL) {
        m_contactInfoDialog = new ContactInfoDialog(this);
        m_contactInfoDialog->setData(
                m_client->getRoster().getRosterEntry(jidToBareJid(m_jid)).name(),
                m_jid,
                m_vCard
                );
    }
    m_contactInfoDialog->show();
    m_contactInfoDialog->raise();
    m_contactInfoDialog->activateWindow();
}

void ChatWindow::sendFileSlot()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Select File",
                                                    QDesktopServices::storageLocation(QDesktopServices::DesktopLocation));

    if (fileName.isEmpty())
        return;
    emit sendFile(m_jid, fileName);
}
