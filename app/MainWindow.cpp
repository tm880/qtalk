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
#include <QXmppVCardManager.h>
#include "TransferManagerWindow.h"
#include <QMessageBox>
#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>
#include <ContactInfoDialog.h>
#include <QDesktopWidget>
#include <QXmppRosterIq.h>
#include "AddContactDialog.h"
#include "InfoEventStackWidget.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_client(new XmppClient(this)),
    m_rosterModel(new RosterModel(m_client, this)),
    m_rosterTreeView(new QTreeView(this)),
    m_unreadMessageModel(new UnreadMessageModel(this)),
    m_unreadMessageWindow(0),
    m_loginWidget(new LoginWidget(this)),
    m_preferencesDialog(0),
    m_closeToTrayDialog(0),
    m_transferManagerWindow(0),
    m_addContactDialog(0)
{
    ui.setupUi(this);
    readPreferences();

    m_client->getConfiguration().setAutoAcceptSubscriptions(false);

    setupTrayIcon();
    ui.toolBar->setVisible(false);

    m_infoEventStackWidget = new InfoEventStackWidget(m_client, this);
    m_infoEventStackWidget->addSubscribeRequest("test");
    m_infoEventStackWidget->addSubscribeRequest("test2");
    QVBoxLayout *bottomLayout = new QVBoxLayout();
    bottomLayout->addWidget(m_infoEventStackWidget);
    bottomLayout->setMargin(0);
    ui.bottomWrap->setLayout(bottomLayout);

    connect(ui.showEventButton, SIGNAL(clicked()),
            this, SLOT(showEventStack()) );

    m_infoEventStackWidget->setHidden(true);

    m_rosterTreeView->setHeaderHidden(true);
    m_rosterTreeView->setAnimated(true);
    rosterIconResize();
    m_rosterTreeView->setRootIsDecorated(false);
    m_rosterTreeView->setWordWrap(true);
    m_rosterTreeView->setAlternatingRowColors(true);
    m_rosterTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

    ui.stackedWidget->addWidget(m_loginWidget);
    ui.stackedWidget->addWidget(m_rosterTreeView);


    changeToLogin();
    //setCentralWidget(m_loginWidget);

    connect(m_loginWidget, SIGNAL(login()),
            this, SLOT(login()));

    // unread message
    connect(m_unreadMessageModel, SIGNAL(messageCleared()),
            this, SLOT(unreadMessageCleared()));

    // xmpp client
    connect(m_client, SIGNAL(connected()),
            this, SLOT(clientConnected()));
    connect(m_client, SIGNAL(disconnected()),
            this, SLOT(clientDisconnect()));
    connect(m_client, SIGNAL(error(QXmppClient::Error)),
            this, SLOT(clientError(QXmppClient::Error)));
    connect(m_client, SIGNAL(messageReceived(QXmppMessage)),
            this, SLOT(messageReceived(QXmppMessage)) );

    // roster model and view
    connect(m_rosterModel, SIGNAL(parseDone()),
            this, SLOT(changeToRoster()) );
    connect(m_rosterModel, SIGNAL(parseDone()),
            this, SLOT(rosterViewHiddenUpdate()) );
    connect(m_rosterModel, SIGNAL(hiddenUpdate()),
            this, SLOT(rosterViewHiddenUpdate()) );
    connect(m_rosterTreeView, SIGNAL(pressed(const QModelIndex &)),
            this, SLOT(rosterItemClicked(const QModelIndex &)));
    connect(m_rosterTreeView, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(rosterContextMenu(const QPoint&)) );

    // action
    connect(ui.actionPreferences, SIGNAL(triggered()),
            this, SLOT(openPreferencesDialog()));
    connect(ui.actionLogout, SIGNAL(triggered()),
            this, SLOT(logout()));
    connect(ui.actionHideOffline, SIGNAL(triggered(bool)),
            this, SLOT(hideOffline(bool)) );
    connect(ui.actionTransferManager, SIGNAL(triggered()),
            this, SLOT(openTransferWindow()) );
    connect(ui.actionAddContact, SIGNAL(triggered()),
            this, SLOT(actionAddContact()) );
    connect(ui.actionQuit, SIGNAL(triggered()),
            this, SLOT(quit()) );
    connect(ui.actionRemoveContact, SIGNAL(triggered()),
            this, SLOT(actionRemoveContact()) );
    connect(ui.actionStartChat, SIGNAL(triggered()),
            this, SLOT(actionStartChat()) );
    connect(ui.actionContactInfo, SIGNAL(triggered()),
            this, SLOT(actionContactInfo()) );
    connect(ui.actionSubscribe, SIGNAL(triggered()),
            this, SLOT(actionSubscribe()) );
    connect(ui.actionUnsubscribe, SIGNAL(triggered()),
            this, SLOT(actionUnsubsribe()) );
    connect(ui.actionDropSubscribe, SIGNAL(triggered()),
            this, SLOT(actionDropSubsribe()) );
    connect(ui.actionAllowSubcribe, SIGNAL(triggered()),
            this, SLOT(actionAllowSubsribe()) );

    // VCard
    connect(&m_client->getVCardManager(), SIGNAL(vCardReceived(const QXmppVCard&)),
            this, SLOT(vCardReveived(const QXmppVCard&)) );

    // transfer manager
    connect(&m_client->getTransferManager(), SIGNAL(fileReceived(QXmppTransferJob*)),
            this, SLOT(receivedTransferJob(QXmppTransferJob*)) );

    m_rosterTreeView->setModel(m_rosterModel);

    if (m_preferences.autoLogin)
        login();
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

    if (m_preferences.mainWindowGeometry.isEmpty())
        move(QApplication::desktop()->screenGeometry().center() - geometry().center());
    else
        restoreGeometry(m_preferences.mainWindowGeometry);
    restoreState(m_preferences.mainWindowState);
}

void MainWindow::writePreferences()
{
    if (m_loginWidget->isVisible())
        m_loginWidget->writeData(&m_preferences);

    m_preferences.mainWindowGeometry = saveGeometry();
    m_preferences.mainWindowState = saveState();

    m_preferences.save();
}

void MainWindow::login()
{
    m_loginWidget->lock();
    m_loginWidget->writeData(&m_preferences);
    m_loginWidget->showState("Login ...");
    //m_client->connectToServer("talk.google.com", "chloerei", "1110chloerei", "gmail.com");
    m_client->connectToServer(m_preferences.host, m_preferences.jid,
                              m_preferences.password, m_preferences.port);
}

void MainWindow::clientConnected()
{
    m_loginWidget->showState("Connect successful");
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

void MainWindow::rosterItemClicked(const QModelIndex &index)
{
    if (QApplication::mouseButtons() == Qt::LeftButton) {
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
        // new chatWindow
        chatWindow = new ChatWindow(jid, m_client, this);

        connect(chatWindow, SIGNAL(sendFile(QString,QString)),
                this, SLOT(createTransferJob(QString,QString)) );
        connect(chatWindow, SIGNAL(viewContactInfo(QString)),
                this, SLOT(openContactInfoDialog(QString)) );

        if (m_rosterModel->hasVCard(jidToBareJid(jid)))
            chatWindow->setVCard(m_rosterModel->getVCard(jidToBareJid(jid)));

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

        // move to screan center
        chatWindow->move(QApplication::desktop()->screenGeometry().center() - chatWindow->geometry().center());
    } else {
        // exist
        chatWindow = m_chatWindows[jid];
    }
    chatWindow->readPref(&m_preferences);

    chatWindow->show();
    chatWindow->raise();
    chatWindow->activateWindow();
}

void MainWindow::actionStartChat()
{
    openChatWindow(m_rosterModel->jidAt(m_rosterTreeView->currentIndex()));
}

void MainWindow::actionContactInfo()
{
    openContactInfoDialog(m_rosterModel->jidAt(m_rosterTreeView->currentIndex()));
}

void MainWindow::actionAddContact()
{
    if (m_addContactDialog == 0) {
        m_addContactDialog = new AddContactDialog(this);
    }

    m_addContactDialog->setGroups(m_rosterModel->getGroups());

    if (m_addContactDialog->exec()) {
        QXmppRosterIq::Item item;
        item.setBareJid(m_addContactDialog->jid());
        if (!m_addContactDialog->name().isEmpty())
            item.setName(m_addContactDialog->name());
        if (!m_addContactDialog->group().isEmpty()) {
            QSet<QString> groups;
            groups << m_addContactDialog->group();
            item.setGroups(groups);
        }
        QXmppRosterIq iq;
        iq.setType(QXmppIq::Set);
        iq.addItem(item);
        m_client->sendPacket(iq);

        QXmppPresence presence(QXmppPresence::Subscribe);
        presence.setTo(m_addContactDialog->jid());
        m_client->sendPacket(presence);
    }
}

void MainWindow::actionRemoveContact()
{
    QString bareJid = jidToBareJid(m_rosterModel->jidAt(m_rosterTreeView->currentIndex()));
    if (QMessageBox::warning(this, QString(tr("Remove Contact")),
                         QString(tr("Are you sure to remove contact: %1 ?")).arg(bareJid),
                         QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Yes) {
        QXmppRosterIq::Item item;
        item.setBareJid(bareJid);
        item.setSubscriptionType(QXmppRosterIq::Item::Remove);
        QXmppRosterIq iq;
        iq.setType(QXmppIq::Set);
        iq.addItem(item);
        m_client->sendPacket(iq);
    }
}

void MainWindow::actionSubscribe()
{
    QString bareJid = jidToBareJid(m_rosterModel->jidAt(m_rosterTreeView->currentIndex()));
    QXmppPresence presence(QXmppPresence::Subscribe);
    presence.setTo(bareJid);
    m_client->sendPacket(presence);
}

void MainWindow::actionUnsubsribe()
{
    QString bareJid = jidToBareJid(m_rosterModel->jidAt(m_rosterTreeView->currentIndex()));
    QXmppPresence presence(QXmppPresence::Unsubscribe);
    presence.setTo(bareJid);
    m_client->sendPacket(presence);
}

void MainWindow::actionDropSubsribe()
{
    QString bareJid = jidToBareJid(m_rosterModel->jidAt(m_rosterTreeView->currentIndex()));
    QXmppPresence presence(QXmppPresence::Unsubscribed);
    presence.setTo(bareJid);
    m_client->sendPacket(presence);
}

void MainWindow::actionAllowSubsribe()
{
    QString bareJid = jidToBareJid(m_rosterModel->jidAt(m_rosterTreeView->currentIndex()));
    QXmppPresence presence(QXmppPresence::Subscribed);
    presence.setTo(bareJid);
    m_client->sendPacket(presence);
}

void MainWindow::showEventStack()
{
    if (m_infoEventStackWidget->isVisible())
        m_infoEventStackWidget->setVisible(false);
    else
        m_infoEventStackWidget->setVisible(true);
}

void MainWindow::openContactInfoDialog(QString jid)
{
    QString bareJid = jidToBareJid(jid);
    ContactInfoDialog *dialog;
    if (m_contactInfoDialogs[bareJid] == NULL) {
        dialog = new ContactInfoDialog(this);
        m_contactInfoDialogs[bareJid] = dialog;
        QXmppRoster::QXmppRosterEntry entry = m_client->getRoster().getRosterEntry(bareJid);
        dialog->setData(entry.name(), jid, m_rosterModel->getVCard(bareJid));
        dialog->move(QApplication::desktop()->screenGeometry().center() - dialog->geometry().center());
    } else {
        dialog = m_contactInfoDialogs[bareJid];
    }
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (m_unreadMessageWindow == 0) {
        createUnreadMessageWindow();
    }

    if (reason == QSystemTrayIcon::Trigger) {
        if (m_unreadMessageModel->hasAnyUnread()
            && m_unreadMessageWindow->isHidden()) {
            if (m_unreadMessageModel->rowCount(QModelIndex()) == 1)
                // single
                readAllUnreadMessage();
            else
                m_unreadMessageWindow->show();
        } else {
            if (isVisible()) {
                hide();
            } else {
                //hide();
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

    //m_quitAction = new QAction(tr("&Quit"), this);
    //connect(m_quitAction, SIGNAL(triggered()), this, SLOT(quit()));

    m_trayIconMenu = new QMenu(this);
    //m_trayIconMenu->addAction(m_quitAction);
    m_trayIconMenu->addAction(ui.actionTransferManager);
    m_trayIconMenu->addAction(ui.actionPreferences);
    m_trayIconMenu->addAction(ui.actionQuit);
    m_trayIcon->setContextMenu(m_trayIconMenu);

    connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));

    m_trayIcon->show();
}

void MainWindow::createUnreadMessageWindow()
{
    m_unreadMessageWindow = new UnreadMessageWindow(this);
    m_unreadMessageWindow->move(m_trayIcon->geometry().center() - m_unreadMessageWindow->geometry().center());
    m_unreadMessageWindow->setModel(m_unreadMessageModel);

    connect(m_unreadMessageWindow, SIGNAL(unreadListClicked(const QModelIndex&)),
            this, SLOT(getUnreadListClicked(const QModelIndex&)));
    connect(m_unreadMessageWindow, SIGNAL(readAll()),
            this, SLOT(readAllUnreadMessage()));
}

void MainWindow::changeTrayIcon(TrayIconType type)
{
    switch (type){
        case online:
            m_trayIcon->setIcon(QIcon(":/images/im-user.png"));
            break;
        case newMessage:
            m_trayIcon->setIcon(QIcon(":/images/mail-unread-new.png"));
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
    if (m_preferencesDialog == 0) {
        m_preferencesDialog = new PreferencesDialog(this);

        // preferences dialog
        connect(m_preferencesDialog, SIGNAL(applied()),
                this, SLOT(preferencesApplied()));
        connect(m_preferencesDialog, SIGNAL(rosterIconSizeChanged(int)),
                this, SLOT(setRosterIconSize(int)) );
        connect(m_preferencesDialog, SIGNAL(rosterIconReseze()),
                this, SLOT(rosterIconResize()) );

    }
    m_preferencesDialog->readData(&m_preferences);
    m_preferencesDialog->show();
}

void MainWindow::preferencesApplied()
{
    m_preferencesDialog->writeData(&m_preferences);

    if (m_preferencesDialog->isRosterPrefChanged()) {
        ui.actionHideOffline->setChecked(m_preferences.hideOffline);
        m_rosterModel->readPref(&m_preferences);
        rosterViewHiddenUpdate();
    }

    m_rosterTreeView->setIconSize(QSize(m_preferences.rosterIconSize, m_preferences.rosterIconSize));

    if (m_preferencesDialog->isAccountChanged())
        m_loginWidget->readData(&m_preferences);

    m_preferences.save();
}

void MainWindow::hideOffline(bool hide)
{
    m_preferences.hideOffline = hide;
    m_rosterModel->readPref(&m_preferences);
    rosterViewHiddenUpdate();
}

void MainWindow::changeToLogin()
{
    ui.toolBar->setVisible(false);
    m_loginWidget->unlock();
    ui.stackedWidget->setCurrentIndex(0);
}

void MainWindow::changeToRoster()
{
    ui.toolBar->setVisible(true);
    ui.stackedWidget->setCurrentIndex(1);
    m_rosterTreeView->expandToDepth(0);
}

void MainWindow::setRosterIconSize(int num)
{
    m_rosterTreeView->setIconSize(QSize(num, num));
}

void MainWindow::rosterIconResize()
{
    setRosterIconSize(m_preferences.rosterIconSize);
}

void MainWindow::rosterViewHiddenUpdate()
{
    foreach (QModelIndex contactIndex, m_rosterModel->allIndex()) {
        m_rosterTreeView->setRowHidden(contactIndex.row(),
                                       contactIndex.parent(),
                                       m_rosterModel->isIndexHidden(contactIndex));;
    }
}

void MainWindow::vCardReveived(const QXmppVCard &vCard)
{
    if (m_chatWindows[vCard.from()] != NULL) {
        ChatWindow *window = m_chatWindows[vCard.from()];
        window->setVCard(vCard);
    }
}

void MainWindow::logout()
{
    m_client->disconnect();
    m_rosterModel->clear();
    foreach (ChatWindow *window, m_chatWindows) {
        if (window != NULL)
            window->close();
    }
    m_chatWindows.clear();
}

void MainWindow::quit()
{
    writePreferences();
    qApp->quit();
}

void MainWindow::openTransferWindow()
{
    initTransferWindow();
    m_transferManagerWindow->show();
}

void MainWindow::createTransferJob(const QString &jid, const QString &fileName)
{
    initTransferWindow();

    QString newJid = jid;
    if (jidToResource(jid).isEmpty()) {
        QStringList resources = m_client->getRoster().getAllPresencesForBareJid(jid).keys();
        if (resources.isEmpty()) {
            QMessageBox::warning(0, "Contact Offline", "Can not send file to offline contact.");
            return;
        } else if (resources.count() == 1) {
            // auto select the single resource
            newJid = jid + "/" + resources.at(0);
        } else {
            // let usr select witch resource to send
            QDialog dialog;
            QListWidget *listWidget = new QListWidget(&dialog);
            foreach(QString resource, resources) {
                QListWidgetItem *item = new QListWidgetItem(resource);
                listWidget->addItem(item);
            }
            listWidget->setCurrentRow(0);
            QVBoxLayout *layout = new QVBoxLayout;
            QDialogButtonBox *buttonBox = new QDialogButtonBox(&dialog);
            buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
            connect(buttonBox, SIGNAL(accepted()),
                    &dialog, SLOT(accept()) );
            connect(buttonBox, SIGNAL(rejected()),
                    &dialog, SLOT(reject()) );
            QLabel *label = new QLabel();
            label->setText(tr("<b>Note:</b> Contact logined at multi resources, you must select witch resource to send"));
            label->setWordWrap(true);
            layout->addWidget(label);
            layout->addWidget(listWidget);
            layout->addWidget(buttonBox);
            dialog.setLayout(layout);
            dialog.setWindowTitle(tr("Mutil Resources"));
            if (dialog.exec()) {
                // accept
                newJid = jid + "/" + resources.at(listWidget->currentRow());
            } else {
                // reject
                return;
            }
        }
    }

    m_transferManagerWindow->createTransferJob(newJid, fileName);
    m_transferManagerWindow->show();
}

void MainWindow::receivedTransferJob(QXmppTransferJob *offer)
{
    initTransferWindow();
    m_transferManagerWindow->receivedTransferJob(offer);
}

void MainWindow::rosterContextMenu(const QPoint &position)
{
    QModelIndex index = m_rosterTreeView->indexAt(position);
    if (index.isValid()) {
        QMenu menu;
        RosterModel::ItemType type = m_rosterModel->itemTypeAt(index);
        if (type != RosterModel::group) {
            menu.addAction(ui.actionStartChat);
            menu.addAction(ui.actionContactInfo);
            if (type == RosterModel::contact) {
                menu.addSeparator();
                QMenu *subMenu = menu.addMenu("Roster");
                // * has bug: auto send subcribe presence when type is 'from', hide it.
                QString bareJid = m_rosterModel->jidAt(index);
                QXmppRoster::QXmppRosterEntry entry = m_client->getRoster().getRosterEntry(bareJid);
                switch (entry.subscriptionType()) {
                case QXmppRoster::QXmppRosterEntry::Both:
                    subMenu->addAction(ui.actionUnsubscribe);
                    subMenu->addAction(ui.actionDropSubscribe);
                    break;
                case QXmppRoster::QXmppRosterEntry::To:
                    subMenu->addAction(ui.actionUnsubscribe);
                    subMenu->addAction(ui.actionAllowSubcribe);
                    break;
                case QXmppRoster::QXmppRosterEntry::From:
                    subMenu->addAction(ui.actionSubscribe);
                    subMenu->addAction(ui.actionDropSubscribe);
                    break;
                case QXmppRoster::QXmppRosterEntry::None:
                    subMenu->addAction(ui.actionSubscribe);
                    break;
                default:
                    break;
                }
                subMenu->addSeparator();
                subMenu->addAction(ui.actionRemoveContact);
            }
            menu.exec(m_rosterTreeView->mapToGlobal(position));
        }
    }
}

void MainWindow::initTransferWindow()
{
    if (m_transferManagerWindow == 0) {
        m_transferManagerWindow = new TransferManagerWindow(&m_client->getTransferManager(), this);
        m_transferManagerWindow->move(QApplication::desktop()->screenGeometry().center() - m_transferManagerWindow->geometry().center());
        connect(&m_client->getTransferManager(), SIGNAL(finished(QXmppTransferJob*)),
                m_transferManagerWindow, SLOT(deleteFileHandel(QXmppTransferJob*)) );

    }
}
