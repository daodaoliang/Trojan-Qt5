#include "statusnotifier.h"
#include "mainwindow.h"
#include "pacserver.h"
#include "systemproxyhelper.h"
#include "subscribedialog.h"
#include "subscribemanager.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#ifdef Q_OS_LINUX
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusPendingCall>
#endif

StatusNotifier::StatusNotifier(MainWindow *w, ConfigHelper *ch, SubscribeManager *sm, QObject *parent) :
    QObject(parent),
    window(w),
    helper(ch),
    sbMgr(sm)
{
    systray.setIcon(QIcon(":/icons/icons/trojan-qt5_off.png"));
    systray.setToolTip(QString("Trojan-Qt5"));
    connect(&systray, &QSystemTrayIcon::activated, [=]() {
        updateMenu();
        updateServersMenu();
    });
    connect(&systray, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::DoubleClick) {
            window->showNormal();
            window->activateWindow();
            window->raise();
        }
    });
    minimiseRestoreAction = new QAction(helper->isHideWindowOnStartup() ? tr("Restore") : tr("Minimise"), this);
    connect(minimiseRestoreAction, &QAction::triggered, this, &StatusNotifier::activate);
    initActions();
    initConnections();
    systrayMenu.addAction(minimiseRestoreAction);
    systrayMenu.addAction(QIcon::fromTheme("application-exit", QIcon::fromTheme("exit")), tr("Quit"), qApp, SLOT(quit()));
    systray.setContextMenu(&systrayMenu);
    systray.show();
    if (helper->isAutoUpdateSubscribes()) {
        sbMgr->updateAllSubscribes(false);
    }
}

void StatusNotifier::initActions()
{
    //trojan Status and Toggle Action
    trojanQt5Action = new QAction(tr("Trojan: Off")); // for displaying the status
    trojanQt5Action->setEnabled(false);
    toggleTrojanAction = new QAction(tr("Turn On Trojan"));
    toggleTrojanAction->setShortcut(Qt::CTRL + Qt::Key_T);

    //Mode Menu
    ModeMenu = new QMenu(tr("Mode"));
    ModeGroup = new QActionGroup(this);
    ModeGroup->setExclusive(true);
    disableModeAction = new QAction(tr("Disable system proxy"), ModeGroup);
    pacModeAction = new QAction(tr("PAC"), ModeGroup);
    globalModeAction = new QAction(tr("Global"), ModeGroup);
    advanceModeAction = new QAction(tr("Advance"), ModeGroup);
    disableModeAction->setCheckable(true);
    pacModeAction->setCheckable(true);
    globalModeAction->setCheckable(true);
    advanceModeAction->setCheckable(true);
    ModeMenu->addAction(disableModeAction);
    ModeMenu->addAction(pacModeAction);
    ModeMenu->addAction(globalModeAction);
    //ModeMenu->addAction(advanceModeAction);
    if (helper->getSystemProxySettings() == "pac")
        pacModeAction->setChecked(true);
    else if (helper->getSystemProxySettings() == "global")
        globalModeAction->setChecked(true);
    else if (helper->getSystemProxySettings() == "direct")
        disableModeAction->setChecked(true);
    else if (helper->getSystemProxySettings() == "advance")
        advanceModeAction->setChecked(true);

    //PAC Menu
    pacMenu = new QMenu(tr("PAC"));
    updatePACToBypassLAN = new QAction(tr("Update local PAC from Lan IP list"));
    updatePACToChnWhite = new QAction(tr("Update local PAC from Chn White list"));
    updatePACToChnWhiteAdvanced = new QAction(tr("Update local from Chn Advance White list")); //Advance White list by @zoeysama
    updatePACToChnIP = new QAction(tr("Update local PAC from Chn IP list"));
    updatePACToGFWList = new QAction(tr("Update local PAC from GFWList"));
    updatePACToChnOnly = new QAction(tr("Update local PAC from Chn Only list"));
    copyPACUrl = new QAction(tr("Copy PAC URL"));
    editLocalPACFile = new QAction(tr("Edit local PAC file"));
    editGFWListUserRule = new QAction(tr("Edit user rule for GFWList"));
    pacMenu->addAction(updatePACToBypassLAN);
    pacMenu->addSeparator();
    pacMenu->addAction(updatePACToChnWhite);
    pacMenu->addAction(updatePACToChnWhiteAdvanced);
    pacMenu->addAction(updatePACToChnIP);
    pacMenu->addAction(updatePACToGFWList);
    pacMenu->addSeparator();
    pacMenu->addAction(updatePACToChnOnly);
    pacMenu->addSeparator();
    pacMenu->addAction(copyPACUrl);
    pacMenu->addAction(editLocalPACFile);
    pacMenu->addAction(editGFWListUserRule);

    //server Menu
    serverMenu = new QMenu(tr("Servers"));
    ServerGroup = new QActionGroup(this);
    ServerGroup->setExclusive(true);
    addServerMenu = new QMenu(tr("Add Server"));
    addManually = new QAction(tr("Add Manually"));
    addFromScreenQRCode = new QAction(tr("Scan QRCode on Screen"));
    addFromPasteBoardUri = new QAction(tr("Add From Pasteboard Uri"));
    addServerMenu->addAction(addManually);
    addServerMenu->addAction(addFromScreenQRCode);
    addServerMenu->addAction(addFromPasteBoardUri);

    //subscribe Menu
    subscribeMenu = new QMenu(tr("Servers Subscribe"));
    subscribeSettings = new QAction(tr("Subscribe setting"));
    updateSubscribe = new QAction(tr("Update subscribe Trojan node"));
    updateSubscribeBypass = new QAction(tr("Update subscribe Trojan node(bypass proxy)"));
    subscribeMenu->addAction(subscribeSettings);
    subscribeMenu->addAction(updateSubscribe);
    subscribeMenu->addAction(updateSubscribeBypass);

    copyTerminalProxyCommand = new QAction(tr("Copy terminal proxy command"));
    setProxyToTelegram = new QAction(tr("Set Proxy to Telegram"));

    //setup systray Menu
    systrayMenu.addAction(trojanQt5Action);
    systrayMenu.addAction(toggleTrojanAction);
    systrayMenu.addSeparator();
    systrayMenu.addMenu(ModeMenu);
    systrayMenu.addMenu(pacMenu);
    systrayMenu.addMenu(serverMenu);
    systrayMenu.addMenu(subscribeMenu);
    systrayMenu.addAction(copyTerminalProxyCommand);
    systrayMenu.addAction(setProxyToTelegram);
    systrayMenu.addSeparator();

    connect(toggleTrojanAction, &QAction::triggered, this, &StatusNotifier::onToggleConnection);
    connect(addManually, &QAction::triggered, window, [=]() { window->onAddServerFromSystemTray("manually"); });
    connect(addFromScreenQRCode, &QAction::triggered, window, [=]() { window->onAddServerFromSystemTray("qrcode"); });
    connect(addFromPasteBoardUri, &QAction::triggered, window, [=]() { window->onAddServerFromSystemTray("pasteboard"); });
    connect(ModeGroup, SIGNAL(triggered(QAction*)), this, SLOT(onToggleMode(QAction*)));
    connect(ServerGroup, SIGNAL(triggered(QAction*)), this, SLOT(onToggleServer(QAction*)));
}

void StatusNotifier::initConnections()
{
    PACServer *pacserver = new PACServer();
    connect(updatePACToBypassLAN, &QAction::triggered, pacserver, [=]() { pacserver->typeModify("LAN"); });
    connect(updatePACToChnWhite, &QAction::triggered, pacserver, [=]() { pacserver->typeModify("WHITE"); });
    connect(updatePACToChnWhiteAdvanced, &QAction::triggered, pacserver, [=]() { pacserver->typeModify("WHITE_ADVANCED"); });
    connect(updatePACToChnIP, &QAction::triggered, pacserver, [=]() { pacserver->typeModify("CNIP"); });
    connect(updatePACToGFWList, &QAction::triggered, pacserver, [=]() { pacserver->typeModify("GFWLIST"); });
    connect(updatePACToChnOnly, &QAction::triggered, pacserver, [=]() { pacserver->typeModify("WHITE_R"); });
    connect(copyPACUrl, &QAction::triggered, pacserver, [=]() { pacserver->copyPACUrl(); });
    connect(editLocalPACFile, &QAction::triggered, pacserver, [=]() { pacserver->editLocalPACFile(); });
    connect(editGFWListUserRule, &QAction::triggered, pacserver, [=]() { pacserver->editUserRule(); });
    connect(subscribeSettings, &QAction::triggered, this, [this]() { onTrojanSubscribeSettings(); });
    connect(updateSubscribe, &QAction::triggered, sbMgr, [=]() { sbMgr->updateAllSubscribes(true); });
    connect(updateSubscribeBypass, &QAction::triggered, sbMgr, [=]() { sbMgr->updateAllSubscribes(false); });
    connect(copyTerminalProxyCommand, &QAction::triggered, this, [this]() { onCopyTerminalProxy(); });
    connect(setProxyToTelegram, &QAction::triggered, this, [this]() { onSetProxyToTelegram(); });
}

void StatusNotifier::updateMenu()
{
    if (helper->getSystemProxySettings() == "pac")
        pacModeAction->setChecked(true);
    else if (helper->getSystemProxySettings() == "global")
        globalModeAction->setChecked(true);
    else if (helper->getSystemProxySettings() == "disable")
        disableModeAction->setChecked(true);
    else if (helper->getSystemProxySettings() == "advance")
        advanceModeAction->setChecked(true);
}

void StatusNotifier::updateServersMenu()
{
    QList<TQProfile> serverList = window->getAllServers();
    TQProfile actived = window->getSelectedServer();
    serverMenu->clear();
    serverMenu->addMenu(addServerMenu);
    serverMenu->addSeparator();
    for (int i=0; i<serverList.size(); i++) {
        QAction *action = new QAction(serverList[i].name, ServerGroup);
        action->setCheckable(false);
        action->setIcon(QIcon(":/icons/icons/trojan_off.png"));
        if (serverList[i].toUri() == actived.toUri())
            action->setIcon(QIcon(":/icons/icons/trojan_on.png"));
        serverMenu->addAction(action);
    }
}

void StatusNotifier::onToggleMode(QAction *action)
{
    SystemProxyHelper *sph = new SystemProxyHelper();
    helper->readGeneralSettings();

    if (action == disableModeAction) {
        if (helper->isTrojanOn())
            sph->setSystemProxy(0);
        helper->setSystemProxySettings("direct");
    } else if (action == pacModeAction) {
        if (helper->isTrojanOn()) {
            sph->setSystemProxy(0);
            sph->setSystemProxy(2);
        }
        helper->setSystemProxySettings("pac");
    } else if (action == globalModeAction) {
        if (helper->isTrojanOn()) {
            sph->setSystemProxy(0);
            sph->setSystemProxy(1);
        }
        helper->setSystemProxySettings("global");
    } else if (action == advanceModeAction) {
        helper->setSystemProxySettings("advance");
    }

    if (helper->isTrojanOn())
        changeIcon(true);
    else
        changeIcon(false);
}

void StatusNotifier::onToggleConnection()
{
    if (toggleTrojanAction->text() == tr("Turn Off Trojan"))
        emit toggleConnection(false);
    else
        emit toggleConnection(true);
}

void StatusNotifier::onToggleServer(QAction *actived)
{
    QList<TQProfile> serverList = window->getAllServers();
    for (int i=0; i<serverList.size(); i++)
        if (actived->text() == serverList[i].name)
            window->onToggleServerFromSystemTray(serverList[i]);
}

void StatusNotifier::onTrojanSubscribeSettings()
{
    SubscribeDialog *sbDig = new SubscribeDialog(helper);
    sbDig->exec();
}

void StatusNotifier::onCopyTerminalProxy()
{
    QClipboard *board = QApplication::clipboard();
    if (helper->isEnableHttpMode())
        board->setText(QString("export HTTP_PROXY=http://%1:%2; export HTTPS_PROXY=http://%1:%2; export ALL_PROXY=socks5://%3:%4").arg(helper->getHttpAddress()).arg(helper->getHttpPort()).arg(helper->getSocks5Address()).arg(helper->getSocks5Port()));
    else
        board->setText(QString("export HTTP_PROXY=socks5://%1:%2; export HTTPS_PROXY=socks5://%1:%2; export ALL_PROXY=socks5://%1:%2").arg(helper->getSocks5Address()).arg(helper->getSocks5Port()));
}

void StatusNotifier::onSetProxyToTelegram()
{
    QDesktopServices::openUrl(QString("tg://socks?server=%1&port=%2").arg(helper->getSocks5Address()).arg(helper->getSocks5Port()));
}

void StatusNotifier::activate()
{
    if (!window->isVisible() || window->isMinimized()) {
        window->showNormal();
        window->activateWindow();
        window->raise();
    } else {
        window->hide();
    }
}

void StatusNotifier::showNotification(const QString &msg)
{
    if (helper->isEnableNotification()) {
#ifdef Q_OS_LINUX
        //Using DBus to send message.
        QDBusMessage method = QDBusMessage::createMethodCall("org.freedesktop.Notifications","/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");
        QVariantList args;
        args << QCoreApplication::applicationName() << quint32(0) << "trojan-qt5" << "Trojan-Qt5" << msg << QStringList () << QVariantMap() << qint32(-1);
        method.setArguments(args);
        QDBusConnection::sessionBus().asyncCall(method);
#else
        systray.showMessage("Trojan-Qt5", msg);
#endif
    }
}

void StatusNotifier::changeIcon(bool started)
{
    if (started) {
        trojanQt5Action->setText(tr("Trojan: On"));
        toggleTrojanAction->setText(tr("Turn Off Trojan"));
        QString mode = helper->getSystemProxySettings();
        QIcon icon(QString(QString(":/icons/icons/trojan-qt5_%1.png").arg(mode)));
        icon.setIsMask(true);
        systray.setIcon(icon);
    } else {
        trojanQt5Action->setText(tr("Trojan: Off"));
        toggleTrojanAction->setText(tr("Turn On Trojan"));
        systray.setIcon(QIcon(":/icons/icons/trojan-qt5_off.png"));
    }
}

void StatusNotifier::onWindowVisibleChanged(bool visible)
{
    minimiseRestoreAction->setText(visible ? tr("Minimise") : tr("Restore"));
}
