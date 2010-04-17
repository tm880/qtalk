#include "MainWindow.h"
#include "XmppClient.h"
#include "QXmppRoster.h"
#include "ChatWindow.h"
#include <QCloseEvent>
#include "QXmppMessage.h"
#include "QXmppUtils.h"
#include <QListView>
#include <QTreeView>
#include "UnreadMessageWindow.h"
#include "UnreadMessageModel.h"
#include "LoginWidget.h"
#include <QSettings>
#include "PreferencesDialog.h"
#include "CloseNoticeDialog.h"
#include "RosterModel.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_client(new XmppClient(this)),
    m_rosterModel(new RosterModel(this)),
    m_rosterTreeView(new QTreeView(this)),
    m_unreadMessageModel(new UnreadMessageModel(this)),
    m_unreadMessageWindow(new UnreadMessageWindow(this)),
    m_loginWidget(new LoginWidget(this)),
    m_preferencesDialog(new PreferencesDialog(this)),
    m_closeToTrayDialog(0)
{
    ui.setupUi(this);
    readPreferences();

    setupTrayIcon();

    m_rosterTreeView->setHeaderHidden(true);
    m_rosterTreeView->setAnimated(true);
    //m_rosterTreeView->setExpandsOnDoubleClick(false);
    m_rosterTreeView->setIconSize(QSize(64, 64));
    m_rosterTreeView->setRootIsDecorated(false);
    ui.stackedWidget->addWidget(m_loginWidget);
    ui.stackedWidget->addWidget(m_rosterTreeView);

    m_rosterModel->setClient(m_client);

    changeToLogin();
    //setCentralWidget(m_loginWidget);

    m_unreadMessageWindow->setModel(m_unreadMessageModel);

    connect(m_loginWidget, SIGNAL(login()),
            this, SLOT(login()));
    connect(m_unreadMessageWindow, SIGNAL(unreadListClicked(const QModelIndex&)),
            this, SLOT(getUnreadListClicked(const QModelIndex&)));

    connect(m_unreadMessageModel, SIGNAL(messageCleared()),
            this, SLOT(unreadMessageCleared()));
    connect(m_unreadMessageWindow, SIGNAL(readAll()),
            this, SLOT(readAllUnreadMessage()));

    connect(m_client, SIGNAL(connected()),
            this, SLOT(clientConnected()));
    connect(&m_client->getRoster(), SIGNAL(rosterReceived()),
            this, SLOT(rosterReceived()));
    connect(m_client, SIGNAL(messageReceived(const QXmppMessage&)),
            this, SLOT(messageReceived(const QXmppMessage&)));
    connect(m_client, SIGNAL(disconnected()),
            this, SLOT(clientDisconnect()));
    connect(m_client, SIGNAL(error(QXmppClient::Error)),
            this, SLOT(clientError(QXmppClient::Error)));

    connect(m_rosterTreeView, SIGNAL(clicked(const QModelIndex &)),
            this, SLOT(rosterDoubleClicked(const QModelIndex &)));
    connect(ui.actionPreferences, SIGNAL(triggered()),
            this, SLOT(openPreferencesDialog()));
    connect(ui.actionLogout, SIGNAL(triggered()),
            m_client, SLOT(disconnect()));
    connect(ui.actionHideOffline, SIGNAL(triggered(bool)),
            this, SLOT(hideOffline(bool)) );

    connect(m_preferencesDialog, SIGNAL(applied()),
            this, SLOT(preferencesApplied()));

    m_rosterTreeView->setModel(m_rosterModel);

}

MainWindow::~MainWindow()
{
}

void MainWindow::readPreferences()
{
    m_preferences.load();

    // action
    ui.actionHideOffline->setChecked(m_preferences.hideOffline);
    m_rosterModel->readPref(&m_preferences);

    m_loginWidget->readData(&m_preferences);
}

void MainWindow::writePreferences()
{
    if (m_loginWidget->isVisible())
        m_loginWidget->writeData(&m_preferences);

    m_preferences.save();
}

void MainWindow::login()
{
    m_loginWidget->lock();
    m_loginWidget->writeData(&m_preferences);
    m_loginWidget->showState("Login ...");
    //m_client->connectToServer("talk.google.com", "chloerei", "1110chloerei", "gmail.com");
    m_client->connectToServer(m_loginWidget->host(), m_loginWidget->jid(),
                              m_loginWidget->password(), m_loginWidget->port());
}

void MainWindow::clientConnected()
{
    m_loginWidget->showState("Connect successful");
}

void MainWindow::rosterReceived()
{
    changeToRoster();
}

void MainWindow::messageReceived(const QXmppMessage& message)
{
    QString jid = message.from();
    QString bareJid = jidToBareJid(jid);
    QString resource = jidToResource(jid);
    if (m_chatWindows[jid] != NULL) {
        m_chatWindows[jid]->appendMessage(message);
    } else if (m_chatWindows[bareJid] != NULL) {
        m_chatWindows[bareJid]->appendMessage(message);
    } else {
        // ignore state message
        if (!message.body().isEmpty()) {
            m_unreadMessageModel->add(message);
            m_rosterModel->messageUnread(bareJid, resource);

            changeTrayIcon(newMessage);
        }
    }
}

void MainWindow::rosterDoubleClicked(const QModelIndex &index)
{
    RosterModel::ItemType type = m_rosterModel->itemTypeAt(index);
    if (type == RosterModel::contact ||
            type == RosterModel::resource) {
        QString jid = m_rosterModel->jidAt(index);
        openChatWindow(jid);
    } else if (type == RosterModel::group) {
        if (m_rosterTreeView->isExpanded(index)) {
            m_rosterTreeView->collapse(index);
        } else {
            m_rosterTreeView->expand(index);
        }
    }
}

void MainWindow::getUnreadListClicked(const QModelIndex &index)
{
    openChatWindow(m_unreadMessageModel->jidAt(index));

    if (m_unreadMessageModel->rowCount(QModelIndex()) == 0) {
        m_unreadMessageWindow->hide();
    }
}

void MainWindow::openChatWindow(const QString &jid)
{
    ChatWindow *chatWindow;
    if (m_chatWindows[jid] == NULL) {
        chatWindow = new ChatWindow(this);
        chatWindow->setJid(jid);
        chatWindow->setClient(m_client);
        chatWindow->setAttribute(Qt::WA_DeleteOnClose);

        m_chatWindows[jid] = chatWindow;
        chatWindow->setWindowTitle(jid);

        // load unread message
        if (m_unreadMessageModel->hasUnread(jid)) {
            foreach (QXmppMessage message, m_unreadMessageModel->take(jid)) {
                chatWindow->appendMessage(message);
            }
        }

        // clean unread state
        QString resource = jidToResource(jid);
        if (resource.isEmpty()) {
            m_rosterModel->messageReadedAll(jidToBareJid(jid));
        } else {
            // resource
            m_rosterModel->messageReaded(jidToBareJid(jid), jidToResource(jid));
        }
    } else {
        chatWindow = m_chatWindows[jid];
    }
    chatWindow->show();
    chatWindow->raise();
    chatWindow->activateWindow();
}

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        if (m_unreadMessageModel->hasAnyUnread()) {
            m_unreadMessageWindow->show();
        } else {
            if (isActiveWindow()) {
                hide();
            } else {
                hide();
                show();
                raise();
                activateWindow();
            }
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_preferences.closeToTrayNotice) {
        if (m_closeToTrayDialog == 0)
            m_closeToTrayDialog = new CloseNoticeDialog(this);
        m_closeToTrayDialog->readData(&m_preferences);
        if (m_closeToTrayDialog->exec())
            m_closeToTrayDialog->writeDate(&m_preferences);
    }

    if (m_preferences.closeToTray) {
        hide();
        event->ignore();
    } else {
        writePreferences();
        event->accept();
    }
}

void MainWindow::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    changeTrayIcon(online);

    m_quitAction = new QAction(tr("&Quit"), this);
    connect(m_quitAction, SIGNAL(triggered()), this, SLOT(quit()));

    m_trayIconMenu = new QMenu(this);
    m_trayIconMenu->addAction(m_quitAction);
    m_trayIcon->setContextMenu(m_trayIconMenu);

    connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));

    m_trayIcon->show();
}

void MainWindow::changeTrayIcon(TrayIconType type)
{
    switch (type){
        case online:
            m_trayIcon->setIcon(QIcon(":/image/user-online.png"));
            break;
        case newMessage:
            m_trayIcon->setIcon(QIcon(":/image/mail-unread-new.png"));
            break;
        default:
            break;
    }
}

void MainWindow::unreadMessageCleared()
{
    changeTrayIcon(online);
}

void MainWindow::readAllUnreadMessage()
{
    foreach (QString bareJid, m_unreadMessageModel->bareJids()) {
        openChatWindow(bareJid);
    }
    m_unreadMessageWindow->hide();
}

void MainWindow::clientDisconnect()
{
    m_loginWidget->showState("Disconnect");
    changeToLogin();
}

void MainWindow::clientError(QXmppClient::Error)
{
    m_loginWidget->showState("Connect Error");
    changeToLogin();
}

void MainWindow::openPreferencesDialog()
{
    m_preferencesDialog->readData(&m_preferences);
    m_preferencesDialog->show();
}

void MainWindow::preferencesApplied()
{
    m_preferencesDialog->writeData(&m_preferences);

    if (m_preferencesDialog->isRosterPrefChanged()) {
        ui.actionHideOffline->setChecked(m_preferences.hideOffline);
        m_rosterModel->readPref(&m_preferences);
    }

    if (m_preferencesDialog->isAccountChanged())
        m_loginWidget->readData(&m_preferences);

    m_preferences.save();
}

void MainWindow::hideOffline(bool hide)
{
    m_preferences.hideOffline = hide;
    m_rosterModel->readPref(&m_preferences);
}

void MainWindow::changeToLogin()
{
    m_loginWidget->unlock();
    ui.stackedWidget->setCurrentIndex(0);
}

void MainWindow::changeToRoster()
{
    ui.stackedWidget->setCurrentIndex(1);
}

void MainWindow::quit()
{
    writePreferences();
    qApp->quit();
}
